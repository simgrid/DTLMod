/* Copyright (c) 2023-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/StagingMqTransport.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void StagingMqTransport::add_publisher(unsigned int /*publisher_id*/)
{
  set_publisher_put_requests_mq(sg4::Actor::self()->get_name());
}

void StagingMqTransport::create_rendez_vous_points()
{
  auto self = sg4::Actor::self();
  // When a new subscriber joins the stream, create a message queue with each know publishers
  for (const auto& pub : get_engine()->get_publishers()) {
    std::string mq_name = pub->get_name() + "_" + self->get_name() + "_mq";
    XBT_DEBUG("Actor '%s' is creating new message queue '%s'", self->get_cname(), mq_name.c_str());
    mqueues_[mq_name] = sg4::MessageQueue::by_name(mq_name);
  }
}
void StagingMqTransport::get_requests_and_do_put(sg4::ActorPtr publisher)
{
  auto pub_name = publisher->get_name();
  // Wait for the reception of the messages. If something is requested, post a put in the message queue for the
  // corresponding publisher-subscriber couple
  while (not pending_put_requests[pub_name].empty()) {
    auto request     = boost::static_pointer_cast<sg4::Mess>(pending_put_requests[pub_name].wait_any());
    auto* subscriber = request->get_sender();
    auto* req_size   = static_cast<size_t*>(request->get_payload());
    if (*req_size > 0) {
      std::string mq_name = pub_name + "_" + subscriber->get_name() + "_mq";
      XBT_DEBUG("%s received a put request from %s. Put a Message in %s with %lu as payload", pub_name.c_str(),
                subscriber->get_cname(), mq_name.c_str(), *req_size);
      auto mess = mqueues_[mq_name]->put_init(req_size);
      // Add callback to release memory allocated for the payload on completion
      mess->on_this_completion_cb([this, req_size](sg4::Mess const&) { delete req_size; });
      get_engine()->pub_transaction_.push(mess->start());
    }
  }
}
void StagingMqTransport::put(std::shared_ptr<Variable> var, size_t /*simulated_size_in_bytes*/)
{
  // Register who (this actor) writes in this transaction
  auto* e   = get_engine();
  auto tid  = e->get_current_transaction();
  auto self = sg4::Actor::self();
  auto pub_name = self->get_name();
  // Use actor's name as temporary location. It's only half of the MessageQueue Name
  var->add_transaction_metadata(tid, self, pub_name);

  // Each Subscriber will send a put request to each publisher in the Stream. They can request for a certain size if
  // they need something from this publisher or 0 otherwise.
  // Start with posting all asynchronous gets and creating an ActivitySet.
  for (unsigned int i = 0; i < e->get_num_subscribers(); i++)
    pending_put_requests[pub_name].push(get_publisher_put_requests_mq(pub_name)->get_async());
}

void StagingMqTransport::get(std::shared_ptr<Variable> var)
{
  auto* e     = get_engine();
  auto self   = sg4::Actor::self();
  auto blocks = check_selection_and_get_blocks_to_get(var);

  // Prepare messages to send to publishers to indicate them whether they have to send something to this subscriber
  // or not. The payload is 0 by default and will be changed when browsing the blocks to get.
  std::unordered_map<std::string, size_t*> put_requests;
  for (const auto& pub : e->get_publishers())
    put_requests[pub->get_name()] = new size_t(0);

  for (auto [publisher_name, size] : blocks) {
    std::string mq_name = publisher_name + "_" + self->get_name() + "_mq";
    XBT_DEBUG("Have to exchange data of size %llu from '%s' to '%s' using the '%s' message queue", size,
              publisher_name.c_str(), self->get_cname(), mq_name.c_str());

    // Update the payload of the put request to send to this publisher.
    delete put_requests[publisher_name];
    put_requests[publisher_name] = new size_t(size);

    // Add an activity to the transaction.
    e->sub_transaction_.push(mqueues_[mq_name]->get_async());
  }

  // Send the put requests for that get to all publishers in the Stream in a detached mode.
  for (auto [pub, size] : put_requests)
    get_publisher_put_requests_mq(pub)->put_init(size)->detach();
}
/// \endcond

} // namespace dtlmod
