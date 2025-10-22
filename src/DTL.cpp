/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

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
DTL::DTL(const std::string& filename)
{
  mutex_ = sg4::Mutex::create();
  // No configuration file has been provided. Nothing else to do
  if (filename.empty())
    return;

  // Parse the configuration file
  std::ifstream f(filename);
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
    streams_[name]->set_engine_type(type)->set_transport_method(transport_method);

    // Check if metadata must be exported for this stream
    if (stream.contains("export_metadata")) {
      streams_[name]->set_metadata_export();
    }
  }
}

void DTL::internal_connect(sg4::Actor* actor)
{
  if (active_connections_.find(actor) == active_connections_.end()) {
    active_connections_.insert(actor);
    XBT_DEBUG("Connection of %s to internal DTL server: %zu active connections", actor->get_cname(),
              active_connections_.size());
  } else
    XBT_WARN("%s is already connected to the DTL. Check your code. ", actor->get_cname());
}

void DTL::internal_disconnect(sg4::Actor* actor)
{
  if (active_connections_.find(actor) == active_connections_.end()) {
    XBT_WARN("%s is not connected to the DTL. Check your code. ", actor->get_cname());
  } else {
    active_connections_.erase(actor);
    XBT_DEBUG("Disconnection from internal DTL server: %zu active connections", active_connections_.size());
  }
}

__attribute__((noreturn)) void DTL::internal_server_init(std::shared_ptr<DTL> dtl)
{
  XBT_DEBUG("Server is running. waiting for connections");
  auto connect_mq = sg4::MessageQueue::by_name("dtlmod::internal_server_connect");
  auto handler_mq = sg4::MessageQueue::by_name("dtlmod::internal_server_handle");
  do {
    // Wait for a connection/disconnection message on connect_mbox
    bool* connect = nullptr;
    auto mess     = connect_mq->get_async(&connect);
    mess->wait();
    auto* sender = mess->get_sender();
    if (*connect) { // Connection
      dtl->internal_connect(sender);
      // Return a handler on the DTL to the newly connected actor
      handler_mq->put_init(&dtl)->detach();
    } else { // Disconnection
      dtl->internal_disconnect(sender);
      handler_mq->put_init(new bool(true))->detach();
      if (!dtl->has_active_connections())
        XBT_WARN("The DTL has no active connection");
    }
    delete connect;
  } while (true);
}
///\endcond

/*** Public interface ***/

void DTL::create(const std::string& filename)
{
  XBT_DEBUG("Creating the DTL server"); // as a daemon running on the first actor in the platform
  sg4::Engine::get_instance()
      ->get_all_hosts()
      .front()
      ->add_actor("dtlmod::internal_server", DTL::internal_server_init, std::make_shared<DTL>(filename))
      ->daemonize();
}

void DTL::create()
{
  create("");
}

std::shared_ptr<DTL> DTL::connect()
{
  sg4::MessageQueue::by_name("dtlmod::internal_server_connect")->put(new bool(true));
  const auto* handle = sg4::MessageQueue::by_name("dtlmod::internal_server_handle")->get<std::shared_ptr<DTL>>();
  return *handle;
}

void DTL::disconnect()
{
  sg4::MessageQueue::by_name("dtlmod::internal_server_connect")->put(new bool(false));
  sg4::MessageQueue::by_name("dtlmod::internal_server_handle")->get_unique<bool>();
}

std::shared_ptr<Stream> DTL::add_stream(const std::string& name)
{
  // This has to be done in critical section to avoid concurrent creation. First actor to get the lock creates the
  // Stream. Other actors will retrieve it from the map.
  std::unique_lock lock(*mutex_);
  if (streams_.find(name) == streams_.end())
    streams_.try_emplace(name, std::make_shared<Stream>(name, this));
  return streams_[name];
}

std::shared_ptr<Stream> DTL::get_stream_by_name_or_null(const std::string& name) const
{
  auto it = streams_.find(name);
  if (it == streams_.end())
    return nullptr;
  return it->second;
}
} // namespace dtlmod
