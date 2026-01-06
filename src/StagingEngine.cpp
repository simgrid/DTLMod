/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

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
#include "dtlmod/StagingMboxTransport.hpp"
#include "dtlmod/StagingMqTransport.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_staging_engine, dtlmod_engine, "DTL logging about staging Engines");

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
void StagingEngine::create_transport(const Transport::Method& transport_method)
{
  XBT_DEBUG("Create a new Staging Engine");
  if (transport_method == Transport::Method::Mailbox) {
    XBT_DEBUG("Creating mailbox '%s'", get_cname());
    set_transport(std::make_shared<StagingMboxTransport>(this));
  } else if (transport_method == Transport::Method::MQ) {
    XBT_DEBUG("Creating Message Queue '%s'", get_cname());
    set_transport(std::make_shared<StagingMqTransport>(this));
  }
}

void StagingEngine::begin_pub_transaction()
{
  if (!pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    current_pub_transaction_id_++;
    XBT_DEBUG("Publish Transaction %u started by %s", current_pub_transaction_id_, sg4::Actor::self()->get_cname());
    // Start the first transaction by notifying subscribers that all publishers are here
    if (current_pub_transaction_id_ == 1) {
      XBT_DEBUG("Notify subscribers that they can create their rendez-vous points");
      first_pub_transaction_started_->notify_all();
    }
  }

  // Only one publisher has to do this
  std::unique_lock lock(*publishers_.get_mutex());
  if (current_pub_transaction_id_ > 1) { // This is not the first transaction.
    // Wait for the completion of the Publish activities from the previous transaction
    XBT_DEBUG("[T %d] (%d) Wait for the completion of %u publish activities from the previous transaction",
              current_pub_transaction_id_, current_sub_transaction_id_, pub_transaction_.size());
    pub_transaction_.wait_all();
    XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
    XBT_DEBUG("%u sub activities pending", sub_transaction_.size());
    pub_transaction_.clear();
  }

  // Then we wait for all subscribers to be at the same transaction
  while (get_num_subscribers() == 0 || current_pub_transaction_id_ > current_sub_transaction_id_) {
    XBT_DEBUG("Wait for subscribers");
    sub_transaction_started_->wait(lock);
  }
  // Publisher has been notified by subscribers, it can proceed with the transaction");
}

void StagingEngine::end_pub_transaction()
{
  // This is the end of the first transaction, create a barrier
  auto pub_barrier = publishers_.get_or_create_barrier();
  if (pub_barrier) {
    XBT_DEBUG("Barrier created for %zu publishers", publishers_.count());
  }

  // A new pub transaction has been completed, notify subscribers that they can starting getting variables
  if (is_last_publisher() && (completed_pub_transaction_id_ < current_pub_transaction_id_)) {
    completed_pub_transaction_id_++;
    pub_transaction_completed_->notify_all();
  }

  // Wait for the put requests and actually put (asynchrously) comm/mess in Mbox/MQ
  std::static_pointer_cast<StagingTransport>(transport_)->get_requests_and_do_put(sg4::Actor::self());
  XBT_DEBUG("Start publish activities for the transaction");

  if (is_last_publisher()) // Mark this transaction as over
    pub_transaction_in_progress_ = false;
}

void StagingEngine::pub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());
  if (!pub_closing_) {
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
    if (stream_.lock()->does_export_metadata())
      export_metadata_to_file();
  }
}

void StagingEngine::begin_sub_transaction()
{
  if (current_sub_transaction_id_ == 0) { // This is the first transaction
    // Wait for at least one publisher to start a tran
    std::unique_lock lock(*subscribers_.get_mutex());
    while (current_pub_transaction_id_ == 0)
      first_pub_transaction_started_->wait(lock);
    XBT_DEBUG("Publishers have started a transaction, create rendez-vous points");
    // We now know the number of publishers, subscriber can create mailboxes/mqs with publishers
    std::static_pointer_cast<StagingTransport>(transport_)->create_rendez_vous_points();
  }

  if (!sub_transaction_in_progress_) {
    current_sub_transaction_id_++;
    sub_transaction_in_progress_ = true;
  }

  num_subscribers_starting_++;
  XBT_DEBUG("Subscribe Transaction %u started by %s (%u/%lu)", current_sub_transaction_id_,
            sg4::Actor::self()->get_cname(), num_subscribers_starting_.load(), get_num_subscribers());

  // The last subscriber to start a transaction notifies the publishers
  if (num_subscribers_starting_.load() == get_num_subscribers() &&
      current_pub_transaction_id_ == current_sub_transaction_id_) {
    XBT_DEBUG("Notify Publishers that they can start their transaction");
    sub_transaction_started_->notify_all();
  }

  std::unique_lock lock(*subscribers_.get_mutex());
  while (completed_pub_transaction_id_ < current_sub_transaction_id_)
    pub_transaction_completed_->wait(lock);
}

void StagingEngine::end_sub_transaction()
{
  // This is the end of the first transaction, create a barrier
  auto sub_barrier = subscribers_.get_or_create_barrier();
  if (sub_barrier) {
    XBT_DEBUG("Barrier created for %zu subscribers", subscribers_.count());
  }

  if (subscribers_.is_last_at_barrier()) {
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    sub_transaction_.wait_all();
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    sub_transaction_.clear();
  }

  // Prevent subscribers to start a new transaction before this one is really over
  if (subscribers_.is_last_at_barrier())
    // Mark this transaction as over
    sub_transaction_in_progress_ = false;
  // Decrease counter for next iteration
  num_subscribers_starting_--;
  XBT_DEBUG("Subscribe Transaction %u end by %s (%u/%lu)", current_sub_transaction_id_, sg4::Actor::self()->get_cname(),
            num_subscribers_starting_.load(), get_num_subscribers());
}

void StagingEngine::sub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());
  if (!sub_closing_) {
    // I'm the first to close
    sub_closing_ = true;
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    sub_transaction_.wait_all();
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
