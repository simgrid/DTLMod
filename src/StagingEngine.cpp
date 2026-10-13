/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fsmod/FileSystem.hpp>
#include <fsmod/PathUtil.hpp>

#include <simgrid/Exception.hpp>
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
void StagingEngine::cancel_activities()
{
  // Do not empty the sets here: the actors blocked in wait_all() are iterating over them and will do their own
  // cleanup when the cancellation unblocks them.
  cancel_pending_activities(get_pub_transaction());
  cancel_pending_activities(get_sub_transaction());
  // Release the publishers waiting for the put requests of subscribers that will not send them anymore
  get_staging_transport()->cancel_pending_put_requests();
  first_pub_transaction_started_->notify_all();
  sub_transaction_started_->notify_all();
  pub_transaction_completed_->notify_all();
}

// A publisher starts its next transaction, and increments its id, before waiting for the transfers of the previous one
// to complete. Cancelling the transaction of the publisher in the meantime means cancelling the one still in flight
// (i.e., the last completed one), not the next one.
unsigned int StagingEngine::get_transaction_to_cancel(unsigned int transaction_id) const
{
  if (transaction_id > completed_pub_transaction_id_ && completed_pub_transaction_id_ > 0 &&
      not get_pub_transaction().empty())
    return completed_pub_transaction_id_;
  return transaction_id;
}

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

std::shared_ptr<StagingTransport> StagingEngine::get_staging_transport() const
{
  auto transport = std::dynamic_pointer_cast<StagingTransport>(get_transport());
  if (!transport)                                                                                   // LCOV_EXCL_LINE
    throw TransportEngineMismatchException(XBT_THROW_POINT, "Transport is not a StagingTransport"); // LCOV_EXCL_LINE
  return transport;
} // LCOV_EXCL_LINE

void StagingEngine::begin_pub_transaction()
{
  // The next transaction was canceled before it started: skip it, so that the next call starts the following one
  if (!pub_transaction_in_progress_ && is_transaction_canceled(current_pub_transaction_id_ + 1)) {
    current_pub_transaction_id_++;
    throw TransactionCanceledException(XBT_THROW_POINT);
  }

  if (!pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    current_pub_transaction_id_++;
    XBT_DEBUG("Publish Transaction %u started by %s", current_pub_transaction_id_, sg4::Actor::self()->get_cname());
    // Start the first transaction by notifying subscribers that all publishers are here
    if (completed_pub_transaction_id_ == 0) {
      XBT_DEBUG("Notify subscribers that they can create their rendez-vous points");
      first_pub_transaction_started_->notify_all();
    }
  }

  // Only one publisher has to do this
  std::unique_lock lock(*get_publishers().get_mutex());
  if (current_pub_transaction_id_ > 1) { // This is not the first transaction.
    // Wait for the completion of the Publish activities from the previous transaction
    XBT_DEBUG("[T %d] (%d) Wait for the completion of %u publish activities from the previous transaction",
              current_pub_transaction_id_, current_sub_transaction_id_, get_pub_transaction().size());
    try {
      get_pub_transaction().wait_all();
    } catch (const simgrid::Exception&) {
      // Cancelling an activity raises a CancelException if it was not matched yet, a NetworkFailureException if it
      // was already transferring data. Any failure that is not due to a cancellation is not ours to handle.
      if (!is_canceled())
        throw; // LCOV_EXCL_LINE
      drain(get_pub_transaction());
      throw TransactionCanceledException(XBT_THROW_POINT);
    }
    XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
    XBT_DEBUG("%u sub activities pending", get_sub_transaction().size());
    get_pub_transaction().clear();
  }

  // Then we wait for all subscribers to be at the same transaction. Checking the transaction id is not enough: it is
  // shared by the subscribers and incremented by the first of them to start the transaction.
  while (!is_transaction_canceled(current_pub_transaction_id_) &&
         (get_subscribers().is_empty() || current_pub_transaction_id_ > current_sub_transaction_id_ ||
          num_subscribers_starting_.load() < get_subscribers().count())) {
    XBT_DEBUG("Wait for subscribers");
    sub_transaction_started_->wait(lock);
  }
  if (is_transaction_canceled(current_pub_transaction_id_)) {
    // Abort this transaction, so that the next call starts the following one
    pub_transaction_in_progress_ = false;
    throw TransactionCanceledException(XBT_THROW_POINT);
  }
  // Publisher has been notified by subscribers, it can proceed with the transaction
}

void StagingEngine::end_pub_transaction()
{
  // This is the end of the first transaction, create a barrier
  if (auto pub_barrier = get_publishers().get_or_create_barrier())
    XBT_DEBUG("Barrier created for %zu publishers", get_publishers().count());

  // A new pub transaction has been completed, notify subscribers that they can starting getting variables. A canceled
  // transaction is never completed: the subscribers waiting for it then all give up in their begin_transaction().
  if (get_publishers().is_last_at_barrier() && (completed_pub_transaction_id_ < current_pub_transaction_id_) &&
      !is_transaction_canceled(current_pub_transaction_id_)) {
    completed_pub_transaction_id_ = current_pub_transaction_id_;
    pub_transaction_completed_->notify_all();
  }

  // Wait for the put requests and actually put (asynchrously) comm/mess in Mbox/MQ
  auto self     = sg4::Actor::self();
  bool canceled = false;
  try {
    get_staging_transport()->get_requests_and_do_put(self);
    XBT_DEBUG("Start publish activities for the transaction");
    // The transaction may also have been canceled before this publisher put anything, or after it received all the
    // put requests. Then, the subscribers already gave up and the puts just started would never be matched.
    if (is_transaction_canceled(current_pub_transaction_id_))
      throw TransactionCanceledException(XBT_THROW_POINT);
  } catch (const simgrid::Exception&) { // CancelException while waiting for the requests, or the throw above
    if (!is_transaction_canceled(current_pub_transaction_id_))
      throw; // LCOV_EXCL_LINE
    // Abort the transaction for this publisher: forget about the requests it still waits for and the puts it already
    // started.
    get_staging_transport()->drop_pending_put_requests(self->get_name());
    drain(get_pub_transaction());
    canceled = true;
  }

  // Mark this transaction as over. This must be done before the barrier below: the publishers it releases can
  // immediately start their next transaction.
  auto tid                     = current_pub_transaction_id_;
  pub_transaction_in_progress_ = false;
  // All the publishers go through this barrier, even when the transaction is canceled: one that would leave early
  // would block the others here forever.
  if (get_publishers().is_last_at_barrier())
    XBT_DEBUG("All publishers ended transaction %u", tid);
  // A canceled transaction is aborted for all the publishers, not only those that noticed it
  if (canceled || is_transaction_canceled(tid))
    throw TransactionCanceledException(XBT_THROW_POINT);
}

void StagingEngine::pub_close()
{
  auto self = sg4::Actor::self();

  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());
  // A publisher may close without having ever ended a transaction, in which case the barrier used below to detect the
  // last publisher to close does not exist yet. Create it now, while all the publishers are still registered.
  if (auto pub_barrier = get_publishers().get_or_create_barrier())
    XBT_DEBUG("Barrier created for %zu publishers", get_publishers().count());
  if (!pub_closing_) {
    // I'm the first to close
    pub_closing_ = true;
    XBT_DEBUG("[%s] Wait for the completion of %u publish activities from the previous transaction", get_cname(),
              get_pub_transaction().size());
    try {
      get_pub_transaction().wait_all();
    } catch (const simgrid::Exception&) { // CancelException or NetworkFailureException, see begin_pub_transaction()
      if (!is_canceled())
        throw; // LCOV_EXCL_LINE
      cancel_pending_activities(get_pub_transaction());
    }
    get_pub_transaction().clear();
    XBT_DEBUG("[%s] last publish transaction is over", get_cname());
    current_pub_transaction_id_++;
  }
  get_publishers().remove(self);

  if (get_publishers().is_last_at_barrier()) {
    XBT_DEBUG("All publishers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Engine '%s' is now closed for all publishers ", get_cname());
    get_stream()->export_metadata_to_file();
    // No more transactions will ever be produced: release any subscriber blocked (or about to block) waiting for one.
    mark_pub_stream_ended();
    first_pub_transaction_started_->notify_all();
    pub_transaction_completed_->notify_all();
  }
}

// Block until the first publisher opens a transaction. Called only for the very first subscribe transaction, to learn
// how many publishers there are before creating the rendez-vous points. No subscriber counter has been touched yet,
// so the cancel/end-of-stream exits here need no rollback.
void StagingEngine::await_first_pub_transaction()
{
  std::unique_lock lock(*get_subscribers().get_mutex());
  while (!is_transaction_canceled(current_sub_transaction_id_ + 1) && current_pub_transaction_id_ == 0 &&
         !pub_stream_ended())
    first_pub_transaction_started_->wait(lock);
  // The next transaction was canceled before it started: skip it, as in begin_sub_transaction()
  if (!sub_transaction_in_progress_ && is_transaction_canceled(current_sub_transaction_id_ + 1)) {
    current_sub_transaction_id_++;
    throw TransactionCanceledException(XBT_THROW_POINT);
  }
  // All publishers closed before ever completing a transaction: nothing will ever come. Checking the completed (and
  // not the current) transaction id matters, as the first publisher to close increments the current one.
  if (completed_pub_transaction_id_ == 0 && pub_stream_ended())
    throw EndOfStreamException(XBT_THROW_POINT);
  XBT_DEBUG("Publishers have started a transaction, create rendez-vous points");
  // We now know the number of publishers, subscriber can create mailboxes/mqs with publishers
  get_staging_transport()->create_rendez_vous_points();
}

// Block until the publishers have completed the transaction this subscriber is starting. On cancel or end of stream,
// roll back the per-subscriber bookkeeping (this transaction was counted as started) so the shared counters stay
// balanced across subscribers, then throw.
void StagingEngine::await_completed_pub_transaction()
{
  std::unique_lock lock(*get_subscribers().get_mutex());
  while (!is_transaction_canceled(current_sub_transaction_id_) &&
         completed_pub_transaction_id_ < current_sub_transaction_id_ && !pub_stream_ended())
    pub_transaction_completed_->wait(lock);
  // Once the publishers completed the transaction, some subscribers may already be getting their data: they all go on
  // and learn about the cancellation together when they end the transaction.
  if (is_transaction_canceled(current_sub_transaction_id_) &&
      completed_pub_transaction_id_ < current_sub_transaction_id_) {
    sub_transaction_in_progress_ = false;
    num_subscribers_starting_--;
    throw TransactionCanceledException(XBT_THROW_POINT);
  }
  if (completed_pub_transaction_id_ < current_sub_transaction_id_ && pub_stream_ended()) {
    sub_transaction_in_progress_ = false;
    num_subscribers_starting_--;
    throw EndOfStreamException(XBT_THROW_POINT);
  }
}

void StagingEngine::begin_sub_transaction()
{
  // The next transaction was canceled before it started: skip it, so that the next call starts the following one
  if (!sub_transaction_in_progress_ && is_transaction_canceled(current_sub_transaction_id_ + 1)) {
    current_sub_transaction_id_++;
    throw TransactionCanceledException(XBT_THROW_POINT);
  }

  // This is the first transaction of this subscriber, which may not be the first transaction of the stream if another
  // subscriber started before
  auto self = sg4::Actor::self();
  if (subscribers_with_rendez_vous_points_.find(self) == subscribers_with_rendez_vous_points_.end()) {
    await_first_pub_transaction();
    subscribers_with_rendez_vous_points_.insert(self);
  }

  if (!sub_transaction_in_progress_) {
    current_sub_transaction_id_++;
    sub_transaction_in_progress_ = true;
  }

  num_subscribers_starting_++;
  XBT_DEBUG("Subscribe Transaction %u started by %s (%u/%lu)", current_sub_transaction_id_,
            sg4::Actor::self()->get_cname(), num_subscribers_starting_.load(), get_subscribers().count());

  // The last subscriber to start a transaction notifies the publishers
  if (num_subscribers_starting_.load() == get_subscribers().count() &&
      current_pub_transaction_id_ == current_sub_transaction_id_) {
    XBT_DEBUG("Notify Publishers that they can start their transaction");
    sub_transaction_started_->notify_all();
  }

  await_completed_pub_transaction();
}

void StagingEngine::end_sub_transaction()
{
  // This is the end of the first transaction, create a barrier
  if (auto sub_barrier = get_subscribers().get_or_create_barrier())
    XBT_DEBUG("Barrier created for %zu subscribers", get_subscribers().count());

  bool canceled = false;
  if (get_subscribers().is_last_at_barrier()) {
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", get_sub_transaction().size());
    try {
      // The transaction may have been canceled before this subscriber started its gets (which then did nothing), or
      // while it was starting them, too late for these gets to be canceled: they will never complete.
      if (is_transaction_canceled(current_sub_transaction_id_))
        throw TransactionCanceledException(XBT_THROW_POINT);
      get_sub_transaction().wait_all();
      // The transaction may also have been canceled right when the gets completed
      if (is_transaction_canceled(current_sub_transaction_id_))
        throw TransactionCanceledException(XBT_THROW_POINT);
      XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
      get_sub_transaction().clear();
    } catch (const simgrid::Exception&) { // CancelException or NetworkFailureException, see begin_pub_transaction()
      if (!is_canceled())
        throw; // LCOV_EXCL_LINE
      drain(get_sub_transaction());
      canceled = true;
    }
  }

  // Mark this transaction as over and decrease counter for next iteration. This must be done before the barrier below:
  // the subscribers it releases can immediately start their next transaction.
  auto tid                     = current_sub_transaction_id_;
  sub_transaction_in_progress_ = false;
  num_subscribers_starting_--;
  XBT_DEBUG("Subscribe Transaction %u end by %s (%u/%lu)", tid, sg4::Actor::self()->get_cname(),
            num_subscribers_starting_.load(), get_subscribers().count());
  // Prevent subscribers to start a new transaction before this one is really over. All the subscribers go through this
  // barrier, even when the transaction is canceled: one that would leave early would block the others here forever.
  if (get_subscribers().is_last_at_barrier())
    XBT_DEBUG("All subscribers ended transaction %u", tid);
  // A canceled transaction is aborted for all the subscribers, not only the one that waited for the transfers
  if (canceled || is_transaction_canceled(tid))
    throw TransactionCanceledException(XBT_THROW_POINT);
}

void StagingEngine::sub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());
  if (!sub_closing_) {
    // I'm the first to close
    sub_closing_ = true;
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", get_sub_transaction().size());
    try {
      get_sub_transaction().wait_all();
    } catch (const simgrid::Exception&) { // CancelException or NetworkFailureException, see begin_pub_transaction()
      if (!is_canceled())
        throw; // LCOV_EXCL_LINE
      cancel_pending_activities(get_sub_transaction());
    }
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    get_sub_transaction().clear();
  }

  get_subscribers().remove(self);

  if (get_subscribers().is_last_at_barrier()) {
    XBT_DEBUG("All subscribers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Engine '%s' is now closed for all subscribers ", get_cname());
  }
}

/// \endcond

} // namespace dtlmod
