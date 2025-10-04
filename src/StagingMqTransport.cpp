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

void StagingMqTransport::create_rendez_vous_points()
{
  auto publishers      = get_engine()->get_publishers();
  auto subscriber_name = sg4::Actor::self()->get_cname();
  // When a new subscriber joins the stream, create a message queue with each know publishers
  XBT_DEBUG("Actor '%s' is creating new message queues", subscriber_name);
  for (const auto& pub : publishers) {
    std::string mq_name = pub->get_name() + "_" + subscriber_name + "_mq";
    mqueues_[mq_name]   = sg4::MessageQueue::by_name(mq_name);
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
      mess->on_this_completion_cb([req_size](sg4::Mess const&) { delete req_size; });
      get_engine()->pub_transaction_.push(mess->start());
    }
  }
}
void StagingMqTransport::get_rendez_vous_point_and_do_get(const std::string& name)
{
  get_engine()->sub_transaction_.push(mqueues_[name + "_mq"]->get_async());
}
/// \endcond

} // namespace dtlmod
