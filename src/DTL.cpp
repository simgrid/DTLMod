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
DTL::DTL(std::string_view filename)
{
  // No configuration file has been provided. Nothing else to do
  if (filename.empty())
    return;

  // Parse the configuration file
  std::ifstream f{std::string(filename)};
  auto data = nlohmann::json::parse(f);

  // Get the list of declared Streams
  for (auto const& stream : data["streams"]) {
    // Get the Stream name
    auto name = stream["name"].get<std::string>();
    // Parse the Engine type
    Engine::Type type;
    if (stream["engine"]["type"] == "File")
      type = Engine::Type::File;
    else if (stream["engine"]["type"] == "Staging")
      type = Engine::Type::Staging;
    else
      throw UnknownEngineTypeException(XBT_THROW_POINT, "");

    // Parse the Transport Method
    Transport::Method transport_method;
    if (stream["engine"]["transport_method"] == "File")
      transport_method = Transport::Method::File;
    else if (stream["engine"]["transport_method"] == "Mailbox")
      transport_method = Transport::Method::Mailbox;
    else if (stream["engine"]["transport_method"] == "MQ")
      transport_method = Transport::Method::MQ;
    else
      throw UnknownTransportMethodException(XBT_THROW_POINT, "");

    // If the Stream doesn't exist yet, create it
    if (streams_.find(name) == streams_.end())
      streams_.try_emplace(name, std::make_shared<Stream>(name, this));
    // And set its engine type and transport method
    streams_[name]->set_engine_type(type).set_transport_method(transport_method);

    // Check if metadata must be exported for this stream
    if (stream.contains("export_metadata")) {
      streams_[name]->set_metadata_export();
    }
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
      // Return a handler on the DTL to the newly connected actor
      handler_mq->put_init(&dtl)->detach();
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
  const auto* handle = sg4::MessageQueue::by_name("dtlmod::connection_manager_handle")->get<std::shared_ptr<DTL>>();
  return *handle;
}

void DTL::disconnect()
{
  auto payload = std::make_unique<bool>(false);
  sg4::MessageQueue::by_name("dtlmod::connection_manager_connect")->put(payload.release());
  sg4::MessageQueue::by_name("dtlmod::connection_manager_handle")->get_unique<bool>();
}

std::shared_ptr<Stream> DTL::add_stream(std::string_view name)
{
  // This has to be done in critical section to avoid concurrent creation. First actor to get the lock creates the
  // Stream. Other actors will retrieve it from the map.
  std::unique_lock lock(*mutex_);
  std::string name_str(name);
  if (streams_.find(name_str) == streams_.end())
    streams_.try_emplace(name_str, std::make_shared<Stream>(name_str, this));
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
