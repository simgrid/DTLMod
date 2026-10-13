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
  auto publish_actors  = get_engine()->get_publishers().get_actors();
  auto subscriber_name = sg4::Actor::self()->get_cname();
  XBT_DEBUG("Actor '%s' is creating new mailboxes", subscriber_name);
  for (const auto& pub : publish_actors) {
    std::string mbox_name = pub->get_name() + "_" + subscriber_name + "_mbox";
    mboxes_[mbox_name]    = sg4::Mailbox::by_name(mbox_name);
  }
}

void StagingMboxTransport::do_put(const std::string& pub_name, const std::string& sub_name, size_t size)
{
  std::string mbox_name = pub_name + "_" + sub_name + "_mbox";
  XBT_DEBUG("Put a Comm of %zu bytes in %s", size, mbox_name.c_str());
  // Send a static dummy payload - subscribers don't use the actual data, only the simulated transfer size
  static size_t dummy = 0;
  auto comm           = mboxes_[mbox_name]->put_init(&dummy, size);
  get_engine()->get_pub_transaction().push(comm->start());
}

void StagingMboxTransport::get_rendez_vous_point_and_do_get(std::string_view name)
{
  // We use a static dummy buffer since we don't use the actual data in simulation
  static size_t* dummy_buffer;
  get_engine()->get_sub_transaction().push(mboxes_[std::string(name) + "_mbox"]->get_async(&dummy_buffer));
}

/// \endcond

} // namespace dtlmod
