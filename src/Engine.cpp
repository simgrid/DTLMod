/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <fstream>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/FileTransport.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);
XBT_LOG_NEW_SUBCATEGORY(dtl_engine, dtlmod, "DTL logging about Engines");

namespace sg4 = simgrid::s4u;

namespace dtlmod {

////////////////////////////////////////////
///////////// PUBLIC INTERFACE /////////////
////////////////////////////////////////////

/// All put and get operations must take place within a transaction (in a sense close to that used for databases).
/// When multiple actors have opened the same Stream and thus subscribed to the same Engine, only one of them has to
/// do the following when a transaction begins, this function is a no-op for the other subscribers:
/// 1. if no transaction is currently in progress, start one, exit otherwise
/// 2. if this is the first transaction for that Engine, create a synchronization barrier among all the subscribers.
/// 3. Otherwise, wait for the completion of the simulated activities started by the previous transaction.
void Engine::begin_transaction()
{
  is_publisher(sg4::Actor::self()) ? begin_pub_transaction() : begin_sub_transaction();
}

/// The actual data transport is delegated to the Transport method associated to the Engine.
void Engine::put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes)
{
  transport_->put(var, simulated_size_in_bytes);
}

/// The actual data transport is delegated to the Transport method associated to the Engine.
void Engine::get(std::shared_ptr<Variable> var)
{
  transport_->get(var);
}

/// This function first synchronizes all the subscribers thanks to the internal barrier. When the last subscriber
/// enters the barrier, all the simulated activities registered for the current transaction are started.
///
/// Then it marks the transaction as done.
void Engine::end_transaction()
{
  is_publisher(sg4::Actor::self()) ? end_pub_transaction() : end_sub_transaction();
}

/// This function is called by all the actors that have opened that Stream. The first subscriber to enter that
/// function waits for the completion of the activities of last transaction on that Engine. Then all the subscribers
/// are synchronized before the Engine is properly closed (and destroyed).
void Engine::close()
{
  is_publisher(sg4::Actor::self()) ? pub_close() : sub_close();
}

////////////////////////////////////////////
///////////////// INTERNALS ////////////////
////////////////////////////////////////////

/// \cond EXCLUDE_FROM_DOCUMENTATION
void Engine::add_publisher(sg4::ActorPtr actor)
{
  transport_->add_publisher(publishers_.size());
  publishers_.insert(actor);
}

void Engine::add_subscriber(sg4::ActorPtr actor)
{
  transport_->add_subscriber(subscribers_.size());
  subscribers_.insert(actor);
}

void Engine::export_metadata_to_file()
{
  std::string filename = boost::replace_all_copy(name_, "/", "#");
  std::ofstream metadata_export(filename + "#md." +
                                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                                std::ofstream::out);
  for (const auto& [name, v] : stream_->get_all_variables())
    v->get_metadata()->export_to_file(metadata_export);
  metadata_export.close();
}

void Engine::close_stream()
{
  stream_->close();
}
/// \endcond

} // namespace dtlmod
