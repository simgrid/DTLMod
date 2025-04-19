/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/StagingTransport.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void StagingTransport::add_publisher(unsigned int /*publisher_id*/)
{
  set_publisher_put_requests_mq(sg4::Actor::self()->get_name());
}
// Create a message queue to receive request for variable pieces from subscribers
void StagingTransport::set_publisher_put_requests_mq(const std::string& publisher_name)
{
    publisher_put_requests_mq_[publisher_name] = sg4::MessageQueue::by_name(publisher_name);
}

sg4::MessageQueue* StagingTransport::get_publisher_put_requests_mq(const std::string& publisher_name) const
{
  return publisher_put_requests_mq_.at(publisher_name);
}

void StagingTransport::put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes)
{
  // Register who (this actor) writes in this transaction
  auto* e   = get_engine();
  auto tid  = e->get_current_transaction();
  auto self = sg4::Actor::self();
  auto pub_name = self->get_name();
  
  // Use actor's name as temporary location. It's only half of the Mailbox Name
  var->add_transaction_metadata(tid, self, pub_name);

  // Each Subscriber will send a put request to each publisher in the Stream. They can request for a certain size if
  // they need something from this publisher or 0 otherwise.
  // Start with posting all asynchronous gets and creating an ActivitySet.
  for (unsigned int i = 0; i < e->get_num_subscribers(); i++)
    pending_put_requests[pub_name].push(get_publisher_put_requests_mq(pub_name)->get_async());
}

void StagingTransport::get(std::shared_ptr<Variable> var)
{
  auto publishers = get_engine()->get_publishers();
  auto self       = sg4::Actor::self();
  auto blocks     = check_selection_and_get_blocks_to_get(var);

  // Prepare messages to send to publishers to indicate them whether they have to send something to this subscriber
  // or not. The payload is 0 by default and will be changed when browsing the blocks to get.
  std::unordered_map<std::string, size_t*> put_requests;
  for (const auto& pub : publishers)
    put_requests[pub->get_name()] = new size_t(0);

  for (auto [publisher_name, size] : blocks) {
    std::string rdv_name = publisher_name + "_" + self->get_name();
    XBT_DEBUG("Have to exchange data of size %llu from '%s' to '%s' using the '%s' rendez-vous point", size,
              publisher_name.c_str(), self->get_cname(), rdv_name.c_str());

    // Update the payload of the put request to send to this publisher if necessary.
    if (size > 0) {
      delete put_requests[publisher_name];
      put_requests[publisher_name] = new size_t(size);
      // Add an activity to the transaction.
      get_rendez_vous_point_and_do_get(rdv_name);
    }
  }

  // Send the put requests for that get to all publishers in the Stream in a detached mode.
  for (auto [pub, size] : put_requests)
    get_publisher_put_requests_mq(pub)->put_init(size)->detach();
}
/// \endcond

} // namespace dtlmod
