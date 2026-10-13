/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/Mess.hpp>
#include <simgrid/s4u/MessageQueue.hpp>
#include <utility>

#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/StagingTransport.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_staging_transport, dtlmod_transport, "DTL logging about staging-based Transport");

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void StagingTransport::add_publisher(unsigned long /*publisher_id*/)
{
  set_publisher_put_requests_mq(sg4::Actor::self()->get_name());
}
// Create a message queue to receive request for variable pieces from subscribers
void StagingTransport::set_publisher_put_requests_mq(std::string_view publisher_name)
{
  publisher_put_requests_mq_[std::string(publisher_name)] = sg4::MessageQueue::by_name(std::string(publisher_name));
}

sg4::MessageQueue* StagingTransport::get_publisher_put_requests_mq(std::string_view publisher_name) const
{
  return publisher_put_requests_mq_.at(std::string(publisher_name));
}

void StagingTransport::put(const std::shared_ptr<Variable>& var, size_t /* simulated_size_in_bytes*/)
{
  // Register who (this actor) writes in this transaction
  auto* e              = get_engine();
  auto tid             = e->get_current_transaction();
  // Nothing will be requested in a canceled transaction: do not wait for it. The publisher learns about the
  // cancellation when it ends the transaction.
  if (e->is_transaction_canceled(tid))
    return;
  auto self            = sg4::Actor::self();
  const auto& pub_name = self->get_name();

  // Use actor's name as temporary location. It's only half of the Mailbox Name
  var->add_transaction_metadata(tid, self, pub_name);

  // Each Subscriber will send a put request to each publisher in the Stream. They can request for a certain size if
  // they need something from this publisher or 0 otherwise.
  // Start with posting all asynchronous gets and creating an ActivitySet.
  const auto num_subscribers = e->get_subscribers().count();
  for (size_t i = 0; i < num_subscribers; i++)
    pending_put_requests_[pub_name].push(get_publisher_put_requests_mq(pub_name)->get_async());
}

void StagingTransport::get_requests_and_do_put(sg4::ActorPtr publisher)
{
  const auto& pub_name = publisher->get_name();
  auto tid             = get_engine()->get_current_transaction();
  auto& requests       = pending_put_requests_[pub_name];
  // Wait for the reception of the requests. If something is requested, start a put for the corresponding
  // publisher-subscriber couple. This wait is released by a CancelException if the transaction gets canceled.
  while (not requests.empty()) {
    // The transaction may have been canceled while this publisher was starting to wait for the requests, too late for
    // these waits to be canceled: the subscribers will not send these requests anymore.
    if (get_engine()->is_transaction_canceled(tid))
      throw TransactionCanceledException(XBT_THROW_POINT);
    auto request = boost::static_pointer_cast<sg4::Mess>(requests.wait_any());
    // Take ownership of the payload received from the subscriber: the transaction and the size it requests
    std::unique_ptr<std::pair<unsigned int, size_t>> payload(
        static_cast<std::pair<unsigned int, size_t>*>(request->get_payload()));
    const auto& [req_tid, req_size] = *payload;
    xbt_assert(req_tid <= tid, "Received a put request for a future transaction");
    if (req_tid < tid) {
      // Sent for a transaction that was canceled after this publisher stopped waiting for it. Discard it, and keep
      // waiting for the request of that subscriber for the current transaction.
      XBT_DEBUG("%s discards a put request for canceled transaction %u", pub_name.c_str(), req_tid);
      requests.push(get_publisher_put_requests_mq(pub_name)->get_async());
      continue;
    }
    if (req_size > 0) {
      XBT_DEBUG("%s received a put request from %s for %zu bytes", pub_name.c_str(), request->get_sender()->get_cname(),
                req_size);
      do_put(pub_name, request->get_sender()->get_name(), req_size);
    }
  }
}

// A publisher waiting for these requests is released by a CancelException.
void StagingTransport::cancel_pending_put_requests()
{
  for (auto& [pub_name, requests] : pending_put_requests_)
    Engine::cancel_pending_activities(requests);
}

void StagingTransport::drop_pending_put_requests(const std::string& pub_name)
{
  auto& requests = pending_put_requests_[pub_name];
  // Free the requests already received (a canceled request has no payload), then forget about the other ones
  while (auto request = requests.test_any())
    delete static_cast<std::pair<unsigned int, size_t>*>(boost::static_pointer_cast<sg4::Mess>(request)->get_payload());
  Engine::drain(requests);
}

void StagingTransport::get(const std::shared_ptr<Variable>& var)
{
  // Nothing will be sent for a canceled transaction: do not ask for anything. The subscriber learns about the
  // cancellation when it ends the transaction.
  auto tid = get_engine()->get_current_sub_transaction_impl();
  if (get_engine()->is_transaction_canceled(tid))
    return;

  auto publishers = get_engine()->get_publishers().get_actors();
  auto self       = sg4::Actor::self();
  auto blocks     = check_selection_and_get_blocks_to_get(var);

  // Prepare messages to send to publishers to indicate them whether they have to send something to this subscriber
  // or not. The payload is the transaction and the requested size, 0 by default, which will be changed when browsing
  // the blocks to get. The transaction lets the publisher recognize, and discard, a request sent for a transaction that
  // was canceled after the publisher stopped waiting for it.
  std::unordered_map<std::string, std::unique_ptr<std::pair<unsigned int, size_t>>> put_requests;
  for (const auto& pub : publishers)
    put_requests[pub->get_name()] = std::make_unique<std::pair<unsigned int, size_t>>(tid, 0);

  for (const auto& [publisher_name, size] : blocks) {
    std::string rdv_name = publisher_name + "_" + self->get_name();
    XBT_DEBUG("Have to exchange data of size %llu from '%s' to '%s' using the '%s' rendez-vous point", size,
              publisher_name.c_str(), self->get_cname(), rdv_name.c_str());

    // Update the payload of the put request to send to this publisher if necessary.
    if (size > 0) {
      put_requests[publisher_name]->second = size;
      // Add an activity to the transaction.
      get_rendez_vous_point_and_do_get(rdv_name);
    }
  }

  // Send the put requests for that get to all publishers in the Stream in a detached mode.
  for (auto& [pub, request] : put_requests)
    get_publisher_put_requests_mq(pub)->put_init(request.release())->detach();
}
/// \endcond

} // namespace dtlmod
