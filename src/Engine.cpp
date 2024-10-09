/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <chrono>
#include <fstream>
#include <boost/algorithm/string/replace.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/FileTransport.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

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
void Engine::add_publisher(sg4::ActorPtr actor, bool rendez_vous)
{
  transport_->add_publisher(publishers_.size());
  publishers_.insert(actor);

  if (rendez_vous) {
    std::unique_lock<sg4::Mutex> lock(*pub_mutex_);
    while (subscribers_.empty())
      open_sync_->wait(lock);
  }
}

void Engine::add_subscriber(sg4::ActorPtr actor, bool rendez_vous)
{
  subscribers_.insert(actor);
  if (rendez_vous) // Notify publishers that there is a subscriber now
    open_sync_->notify_all();

  // Then block the subscriber until at least one publisher initiates a transaction (and thus creates pub_barrier_)
  std::unique_lock<sg4::Mutex> lock(*sub_mutex_);
  while (not publishers_.empty() && not pub_barrier_)
    first_pub_transaction_started_->wait(lock);

  transport_->add_subscriber(subscribers_.size());
}

void Engine::begin_pub_transaction()
{
  // Only one publisher has to do this
  std::unique_lock<sg4::Mutex> lock(*pub_mutex_);
  if (not pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    if (not pub_barrier_) {          // This is the first transaction.
      if (not publishers_.empty()) { // Assume all publishers have opened the stream and create a barrier
        XBT_DEBUG("Create a barrier for %zu publishers", publishers_.size());
        pub_barrier_ = sg4::Barrier::create(publishers_.size());
        first_pub_transaction_started_->notify_all();
      }

    } else {
      // Wait for the completion of the Publish activities from the previous transaction
      XBT_DEBUG("Wait for the completion of %u publish activities from the previous transaction",
                pub_transaction_.size());
      pub_transaction_.wait_all();
      XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
      pub_transaction_id_++;
      XBT_DEBUG("%u sub activities pending", sub_transaction_.size());
      if (pub_transaction_id_ >= sub_transaction_id_) {
      pub_transaction_.clear();
        // We may have subscribers waiting for a transaction to be over. Notify them
        pub_transaction_completed_->notify_all();
      }
    }
    XBT_DEBUG("Publish Transaction %u started by %s", pub_transaction_id_, sg4::Actor::self()->get_cname());
  }
  if (type_ == Type::Staging) {
     XBT_DEBUG("Maybe I should wait: %zu subscribers and %u <= %u" , get_num_subscribers(), pub_transaction_id_, sub_transaction_id_ -1);
     while (get_num_subscribers() == 0 || pub_transaction_id_ < sub_transaction_id_ -1 ) {
       XBT_DEBUG("Wait");
       sub_transaction_started_->wait(lock);
     }

  }
}

void Engine::end_pub_transaction()
{
  if (pub_barrier_ && pub_barrier_->wait()) { // I'm the last publisher entering the barrier
    XBT_DEBUG("Start the %d publish activities for the transaction", pub_transaction_.size());
    for (unsigned int i = 0; i < pub_transaction_.size(); i++)
      pub_transaction_.at(i)->resume();

    // Mark this transaction as over
    pub_transaction_in_progress_ = false;
  }
}

void Engine::pub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());
  if (not pub_closing_) {
    // I'm the first to close
    pub_closing_ = true;
    XBT_DEBUG("[%s] Wait for the completion of %u publish activities from the previous transaction",
              get_cname(), pub_transaction_.size());
    pub_transaction_.wait_all();
    pub_transaction_.clear();
    XBT_DEBUG("[%s] last publish transaction is over", get_cname());
    pub_transaction_id_++;
    if (type_ == Type::File) {
      if (get_num_subscribers() > 0 && pub_transaction_id_ >= sub_transaction_id_) {
        // We may have subscribers waiting for a transaction to be over. Notify them
        pub_transaction_completed_->notify_all();
      }
    }
  }
  rm_publisher(self);
  // Synchronize publishers on engine closing
  if (pub_barrier_ && pub_barrier_->wait()) {
    XBT_DEBUG("All publishers have called the Engine::close() function");

//    XBT_DEBUG("Export metadata");
//    export_metadata_to_file();
    stream_->close();
    if (std::dynamic_pointer_cast<FileTransport>(transport_) != nullptr) {
      XBT_DEBUG("Closing opened files");
      std::static_pointer_cast<FileTransport>(transport_)->close_files();
    }
  }
  // Notify subscribers that may hanging on the beginning of a first transaction ... which never happened.
  first_pub_transaction_started_->notify_all();

  XBT_DEBUG("Engine '%s' is now closed for all publishers ", get_cname());
  pub_closing_ = false;
}

void Engine::begin_sub_transaction()
{
  // Only one subscriber has to do this
  std::unique_lock<sg4::Mutex> lock(*sub_mutex_);

  // We have publishers on that stream, wait for them to complete a transaction first
  if (type_ == Type::File && get_num_publishers() > 0) {
    while (pub_transaction_id_ < sub_transaction_id_)
      pub_transaction_completed_->wait(lock);
  }
  if (not sub_transaction_in_progress_) {
    if (type_ == Type::Staging && pub_transaction_id_ == sub_transaction_id_ -1)
      sub_transaction_started_->notify_all();
    XBT_DEBUG("Subscribe Transaction %u started by %s", sub_transaction_id_, sg4::Actor::self()->get_cname());
    sub_transaction_in_progress_ = true;
    if (not sub_barrier_) {           // This is the first transaction.
      if (not subscribers_.empty()) { // Assume all subscribers have opened the stream and create a barrier
        XBT_DEBUG("Create a barrier for %zu subscribers", subscribers_.size());
        sub_barrier_ = sg4::Barrier::create(subscribers_.size());
      }
    }
  }
}

void Engine::end_sub_transaction()
{
  if (sub_barrier_ && sub_barrier_->wait()) { // I'm the last subscriber entering the barrier
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    for (unsigned int i = 0; i < sub_transaction_.size(); i++)
      sub_transaction_.at(i)->resume()->wait();
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    sub_transaction_.clear();
    // Mark this transaction as over
    sub_transaction_in_progress_ = false;
    sub_transaction_id_++;
  }
  // Prevent subscribers to start a new transaction before this one is really over
  if (sub_barrier_)
    sub_barrier_->wait();
}

void Engine::sub_close()
{
  auto self               = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());
  if (not sub_closing_) {
    // I'm the first to close
    sub_closing_ = true;
  }
  rm_subscriber(self);

  // Synchronize subscribers on engine closing
  if (sub_barrier_ && sub_barrier_->wait()) {
    XBT_DEBUG("All subscribers have called the Engine::close() function");
    stream_->close();
    if (std::dynamic_pointer_cast<FileTransport>(transport_) != nullptr) {
      XBT_DEBUG("Closing opened files");
      std::static_pointer_cast<FileTransport>(transport_)->close_files();
    }
  }
  XBT_DEBUG("Engine '%s' is now closed for all subscribers ", get_cname());
  sub_closing_ = false;
}

void Engine::export_metadata_to_file()
{
  std::string filename = boost::replace_all_copy(name_, "/", "#");
  std::ofstream metadata_export(filename + "#md." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                                std::ofstream::out);
  for (const auto& [name, v] : stream_->get_all_variables())
    v->get_metadata()->export_to_file(metadata_export);
  metadata_export.close();
}
/// \endcond

} // namespace dtlmod
