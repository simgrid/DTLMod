/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/Mess.hpp>

#include "dtlmod/DTLException.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/StagingMboxTransport.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod_staging_transport);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void StagingMboxTransport::create_rendez_vous_points()
{
  auto publishers      = get_engine()->get_publishers();
  auto subscriber_name = sg4::Actor::self()->get_cname();
  XBT_DEBUG("Actor '%s' is creating new mailboxes", subscriber_name);
  for (const auto& pub : publishers) {
    std::string mbox_name = pub->get_name() + "_" + subscriber_name + "_mbox";
    mboxes_[mbox_name]    = sg4::Mailbox::by_name(mbox_name);
  }
}

void StagingMboxTransport::get_requests_and_do_put(sg4::ActorPtr publisher)
{
  auto pub_name = publisher->get_name();
  // Wait for the reception of the messages. If something is requested, post a put in the mailbox for the
  // corresponding publisher-subscriber couple
  while (pending_put_requests_exist_for(pub_name)) {
    auto request           = boost::static_pointer_cast<sg4::Mess>(wait_any_pending_put_request_for(pub_name));
    const auto* subscriber = request->get_sender();
    auto* req_size         = static_cast<size_t*>(request->get_payload());
    if (*req_size > 0) {
      std::string mbox_name = pub_name + "_" + subscriber->get_name() + "_mbox";
      XBT_DEBUG("%s received a put request from %s. Put a Message in %s with %lu as payload", pub_name.c_str(),
                subscriber->get_cname(), mbox_name.c_str(), *req_size);

      auto comm = mboxes_[mbox_name]->put_init(req_size, *req_size);
      // Add callback to release memory allocated for the payload on completion
      comm->on_this_completion_cb([req_size](sg4::Comm const&) { delete req_size; });
      get_engine()->get_pub_transaction().push(comm->start());
    }
  }
}

void StagingMboxTransport::get_rendez_vous_point_and_do_get(const std::string& name)
{
  get_engine()->get_sub_transaction().push(mboxes_[name + "_mbox"]->get_async());
}

/// \endcond

} // namespace dtlmod
