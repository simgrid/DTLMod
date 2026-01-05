/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <fstream>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/FileTransport.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_engine, dtlmod, "DTL logging about Engines");

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
void Engine::put(const std::shared_ptr<Variable>& var) const
{
  if (var->is_reduced()) {
    // Perform an Exec activity before putting the variable into the DTL to account for the time needed to reduce it.
    sg4::this_actor::execute(var->get_reduction_method()->get_flop_amount_to_reduce_variable(var));
    XBT_DEBUG("Variable %s has been reduced!", var->get_cname());
    // Now put the reduced version of the variable into the DTL, i.e., using its reduced local size.
    XBT_DEBUG("Put this reduced version of %s (initial size = %lu, reduced size = %lu)", var->get_cname(),
              var->get_local_size(), var->get_reduction_method()->get_reduced_variable_local_size(var));
    transport_->put(var, var->get_reduction_method()->get_reduced_variable_local_size(var));
  } else
    transport_->put(var, var->get_local_size());
}

void Engine::put(const std::shared_ptr<Variable>& var, size_t simulated_size_in_bytes) const
{
  transport_->put(var, simulated_size_in_bytes);
}

/// The actual data transport is delegated to the Transport method associated to the Engine.
void Engine::get(const std::shared_ptr<Variable>& var) const
{
  if (var->is_reduced() && var->is_reduced_by_subscriber()) {
    var->is_reduced_with_->reduce_variable(var);
    // Perform an Exec activity before putting the variable into the DTL to account for the time needed to reduce it.
    sg4::this_actor::execute(var->get_reduction_method()->get_flop_amount_to_reduce_variable(var));
  }

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

void Engine::export_metadata_to_file() const
{
  std::ofstream metadata_export(metadata_file_, std::ofstream::out);
  for (const auto& [name, v] : stream_.lock()->get_all_variables_internal())
    v->get_metadata()->export_to_file(metadata_export);
  metadata_export.close();
}
void Engine::set_metadata_file_name()
{
  metadata_file_ = boost::replace_all_copy(name_, "/", "#") + "#md." +
                   std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

void Engine::close_stream() const
{
  stream_.lock()->close();
}
/// \endcond

} // namespace dtlmod
