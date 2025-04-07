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
/// \endcond

} // namespace dtlmod
