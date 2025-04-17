/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fsmod/FileSystem.hpp>
#include <fsmod/PathUtil.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
void StagingEngine::create_transport(const Transport::Method& transport_method)
{
  XBT_DEBUG("Create a new Staging Engine");
  if (transport_method == Transport::Method::Mailbox) {
    XBT_DEBUG("Creating mailbox '%s'", get_cname());
    set_transport(std::make_shared<MailboxTransport>(get_name() + "-mbox", this));
  } else if (transport_method == Transport::Method::MQ) {
    XBT_DEBUG("Creating Message Queue '%s'", get_cname());
    set_transport(std::make_shared<MessageQueueTransport>(get_name() + "-mq", this));
  }
}

void StagingEngine::begin_pub_transaction()
{
  // Only one publisher has to do this
  std::unique_lock<sg4::Mutex> lock(*pub_mutex_);
  if (not pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    if (pub_barrier_) { // This is not the first transaction.
      // Wait for the completion of the Publish activities from the previous transaction
      XBT_DEBUG("Wait for the completion of %u publish activities from the previous transaction",
                pub_transaction_.size());
      pub_transaction_.wait_all();
      XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
      current_pub_transaction_id_++;
      XBT_DEBUG("%u sub activities pending", sub_transaction_.size());
      if (current_pub_transaction_id_ >= sub_transaction_id_) {
        pub_transaction_.clear();
        // We may have subscribers waiting for a transaction to be over. Notify them
        pub_transaction_completed_->notify_all();
      }
    }
    XBT_DEBUG("Publish Transaction %u started by %s", current_pub_transaction_id_, sg4::Actor::self()->get_cname());
  }

  XBT_DEBUG("Maybe I should wait: %zu subscribers and %u <= %u", get_num_subscribers(), current_pub_transaction_id_,
            sub_transaction_id_ - 1);
  while (get_num_subscribers() == 0 || current_pub_transaction_id_ < sub_transaction_id_ - 1) {
    XBT_DEBUG("Wait");
    sub_transaction_started_->wait(lock);
  }
}

void StagingEngine::end_pub_transaction()
{
  // This is the end of the first transaction, create a barrier
  if (not pub_barrier_) {
    XBT_DEBUG("Create a barrier for %zu publishers", publishers_.size());
    pub_barrier_ = sg4::Barrier::create(publishers_.size());
  }

  if (is_last_publisher()) {
    XBT_DEBUG("Start the %d publish activities for the transaction", pub_transaction_.size());
    for (unsigned int i = 0; i < pub_transaction_.size(); i++)
      pub_transaction_.at(i)->resume();

    // Mark this transaction as over
    pub_transaction_in_progress_ = false;
    // A new pub transaction has been completed
    completed_pub_transaction_id_++;
    pub_transaction_completed_->notify_all();
  }
}

void StagingEngine::pub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());
  if (not pub_closing_) {
    // I'm the first to close
    pub_closing_ = true;
    XBT_DEBUG("[%s] Wait for the completion of %u publish activities from the previous transaction", get_cname(),
              pub_transaction_.size());
    pub_transaction_.wait_all();
    pub_transaction_.clear();
    XBT_DEBUG("[%s] last publish transaction is over", get_cname());
    current_pub_transaction_id_++;
  }
  rm_publisher(self);
  if (is_last_publisher()) {
    XBT_DEBUG("All publishers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Engine '%s' is now closed for all publishers ", get_cname());
  }
}

void StagingEngine::begin_sub_transaction()
{
  // Only one subscriber has to do this
  std::unique_lock<sg4::Mutex> lock(*sub_mutex_);

  if (not sub_transaction_in_progress_) {
    if (current_pub_transaction_id_ == sub_transaction_id_ - 1)
      sub_transaction_started_->notify_all();
    XBT_DEBUG("Subscribe Transaction %u started by %s", sub_transaction_id_, sg4::Actor::self()->get_cname());
    sub_transaction_in_progress_ = true;
  }
}

void StagingEngine::end_sub_transaction()
{
  // This is the end of the first transaction, create a barrier
  if (not sub_barrier_) {
    XBT_DEBUG("Create a barrier for %zu subscribers", subscribers_.size());
    sub_barrier_ = sg4::Barrier::create(subscribers_.size());
  }

  if (is_last_subscriber()) {
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    for (unsigned int i = 0; i < sub_transaction_.size(); i++)
      sub_transaction_.at(i)->resume()->wait();
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    sub_transaction_.clear();
    // Mark this transaction as over
    sub_transaction_in_progress_ = false;
    sub_transaction_id_++;
  }
  // FIXME: Should not be necessary
  // Prevent subscribers to start a new transaction before this one is really over
  if (sub_barrier_)
    sub_barrier_->wait();
}

void StagingEngine::sub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());
  if (not sub_closing_) {
    // I'm the first to close
    sub_closing_ = true;
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    for (unsigned int i = 0; i < sub_transaction_.size(); i++)
      sub_transaction_.at(i)->resume()->wait();
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    sub_transaction_.clear();
  }

  rm_subscriber(self);

  if (is_last_subscriber()) {
    XBT_DEBUG("All subscribers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Engine '%s' is now closed for all subscribers ", get_cname());
  }
}

/// \endcond

} // namespace dtlmod
