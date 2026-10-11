/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod, "DTLMod internal logging");
XBT_LOG_NEW_SUBCATEGORY(dtl, dtlmod, "DTL logging");

namespace sg4 = simgrid::s4u;
namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
namespace {
Engine::Type parse_engine_type(const nlohmann::json& engine)
{
  if (engine["type"] == "File")
    return Engine::Type::File;
  if (engine["type"] == "Staging")
    return Engine::Type::Staging;
  throw UnknownEngineTypeException(XBT_THROW_POINT, "");
}

Transport::Method parse_transport_method(const nlohmann::json& engine)
{
  if (engine["transport_method"] == "File")
    return Transport::Method::File;
  if (engine["transport_method"] == "Mailbox")
    return Transport::Method::Mailbox;
  if (engine["transport_method"] == "MQ")
    return Transport::Method::MQ;
  throw UnknownTransportMethodException(XBT_THROW_POINT, "");
}

/// Apply the optional settings of a stream entry of the configuration file to the corresponding Stream.
void configure_stream_options(Stream& stream, const nlohmann::json& config)
{
  const auto& engine = config["engine"];
  // Check if a local memory copy overhead must be simulated before a put on this stream
  if (engine.contains("simulate_memory_copy"))
    stream.set_simulate_memory_copy(engine["simulate_memory_copy"].get<bool>());

  // Check if a fixed delay must be simulated each time an actor opens this stream
  if (engine.contains("open_overhead"))
    stream.set_open_overhead(engine["open_overhead"].get<double>());

  // Check if a reduction method must be defined for the stream
  if (config.contains("reduction_methods"))
    for (const auto& method : config["reduction_methods"])
      stream.define_reduction_method(method.get<std::string>());

  // Check if metadata must be exported for this stream
  if (config.contains("export_metadata") && config["export_metadata"].get<bool>())
    stream.set_metadata_export();
}
} // namespace

DTL::DTL(std::string_view filename)
{
  // No configuration file has been provided. Nothing else to do
  if (filename.empty())
    return;

  // Parse the configuration file
  std::ifstream f{std::string(filename)};
  auto data = nlohmann::json::parse(f);

  // Get the list of declared Streams
  for (auto const& config : data["streams"]) {
    auto name             = config["name"].get<std::string>();
    auto type             = parse_engine_type(config["engine"]);
    auto transport_method = parse_transport_method(config["engine"]);

    // If the Stream doesn't exist yet, create it
    if (streams_.find(name) == streams_.end())
      streams_.try_emplace(name, std::make_shared<Stream>(name, this));
    // And set its engine type and transport method
    auto& stream = *streams_[name];
    stream.set_engine_type(type).set_transport_method(transport_method);
    configure_stream_options(stream, config);
  }
}

void DTL::connection_manager_connect(sg4::Actor* actor)
{
  if (active_connections_.find(actor) == active_connections_.end()) {
    active_connections_.insert(actor);
    XBT_DEBUG("Connection of %s to internal DTL server: %zu active connections", actor->get_cname(),
              active_connections_.size());
  } else
    XBT_WARN("%s is already connected to the DTL. Check your code. ", actor->get_cname());
}

void DTL::connection_manager_disconnect(sg4::Actor* actor)
{
  if (active_connections_.find(actor) == active_connections_.end()) {
    XBT_WARN("%s is not connected to the DTL. Check your code. ", actor->get_cname());
  } else {
    active_connections_.erase(actor);
    XBT_DEBUG("Disconnection from internal DTL server: %zu active connections", active_connections_.size());
  }
}

__attribute__((noreturn)) void DTL::connection_manager_init(std::shared_ptr<DTL> dtl)
{
  XBT_DEBUG("Connection_manager is running. waiting for connections");
  auto connect_mq = sg4::MessageQueue::by_name("dtlmod::connection_manager_connect");
  auto handler_mq = sg4::MessageQueue::by_name("dtlmod::connection_manager_handle");
  do {
    // Wait for a connection/disconnection message on connect_mq
    bool* connect = nullptr;
    auto mess     = connect_mq->get_async(&connect);
    mess->wait();
    std::unique_ptr<bool> connect_guard(connect); // RAII: automatic cleanup
    auto* sender = mess->get_sender();
    if (*connect_guard) { // Connection
      dtl->connection_manager_connect(sender);
      // Transfer ownership via unique_ptr for exception safety
      auto dtl_copy = std::make_unique<std::shared_ptr<DTL>>(dtl);
      handler_mq->put_init(dtl_copy.release())->detach();
    } else { // Disconnection
      dtl->connection_manager_disconnect(sender);
      auto payload = std::make_unique<bool>(true);
      handler_mq->put_init(payload.release())->detach();
      if (!dtl->has_active_connections())
        XBT_WARN("The DTL has no active connection");
    }
    // connect_guard automatically deletes connect at end of scope
  } while (true);
}
///\endcond

/*** Public interface ***/

void DTL::create(std::string_view filename)
{
  XBT_DEBUG("Creating the DTL connection manager"); // as a daemon running on the first actor in the platform
  sg4::Engine::get_instance()
      ->get_all_hosts()
      .front()
      ->add_actor("dtlmod::connection_manager", DTL::connection_manager_init, std::make_shared<DTL>(filename))
      ->daemonize();
}

std::shared_ptr<DTL> DTL::connect()
{
  auto payload = std::make_unique<bool>(true);
  sg4::MessageQueue::by_name("dtlmod::connection_manager_connect")->put(payload.release());
  auto handle = sg4::MessageQueue::by_name("dtlmod::connection_manager_handle")->get_unique<std::shared_ptr<DTL>>();
  return *handle;
}

void DTL::disconnect()
{
  auto payload = std::make_unique<bool>(false);
  sg4::MessageQueue::by_name("dtlmod::connection_manager_connect")->put(payload.release());
  sg4::MessageQueue::by_name("dtlmod::connection_manager_handle")->get_unique<bool>();
}

std::shared_ptr<Stream> DTL::add_stream(std::string_view name, Engine::Type type, Transport::Method method)
{
  // This has to be done in critical section to avoid concurrent creation. First actor to get the lock creates the
  // Stream. Other actors will retrieve it from the map.
  std::unique_lock lock(*mutex_);
  std::string name_str(name);
  if (streams_.find(name_str) == streams_.end()) {
    streams_.try_emplace(name_str, std::make_shared<Stream>(name_str, this));
    if (type != Engine::Type::Undefined)
      streams_[name_str]->set_engine_type(type);
    if (method != Transport::Method::Undefined)
      streams_[name_str]->set_transport_method(method);
  }
  return streams_[name_str];
}

std::optional<std::shared_ptr<Stream>> DTL::get_stream_by_name(std::string_view name) const
{
  auto it = streams_.find(std::string(name));
  if (it == streams_.end())
    return std::nullopt;
  return it->second;
}
} // namespace dtlmod
