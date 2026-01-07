/* Copyright (c) 2023-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/StagingMqTransport.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod_staging_transport);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void StagingMqTransport::create_rendez_vous_points()
{
  auto publish_actors  = get_engine()->get_publishers().get_actors();
  auto subscriber_name = sg4::Actor::self()->get_cname();
  // When a new subscriber joins the stream, create a message queue with each know publishers
  XBT_DEBUG("Actor '%s' is creating new message queues", subscriber_name);
  for (const auto& pub : publish_actors) {
    std::string mq_name = pub->get_name() + "_" + subscriber_name + "_mq";
    mqueues_[mq_name]   = sg4::MessageQueue::by_name(mq_name);
  }
}

void StagingMqTransport::get_requests_and_do_put(sg4::ActorPtr publisher)
{
  auto pub_name = publisher->get_name();
  // Wait for the reception of the messages. If something is requested, post a put in the message queue for the
  // corresponding publisher-subscriber couple
  while (pending_put_requests_exist_for(pub_name)) {
    auto request           = boost::static_pointer_cast<sg4::Mess>(wait_any_pending_put_request_for(pub_name));
    const auto* subscriber = request->get_sender();
    auto* req_size         = static_cast<size_t*>(request->get_payload());
    if (*req_size > 0) {
      std::string mq_name = pub_name + "_" + subscriber->get_name() + "_mq";
      XBT_DEBUG("%s received a put request from %s. Put a Message in %s with %lu as payload", pub_name.c_str(),
                subscriber->get_cname(), mq_name.c_str(), *req_size);
      auto mess = mqueues_[mq_name]->put_init(req_size);
      get_engine()->get_pub_transaction().push(mess->start());
    }
  }
}
void StagingMqTransport::get_rendez_vous_point_and_do_get(const std::string& name)
{
  get_engine()->get_sub_transaction().push(mqueues_[name + "_mq"]->get_async());
}
/// \endcond

} // namespace dtlmod
