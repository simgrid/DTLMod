/* Copyright (c) 2022-2023. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_MAILBOX_TRANSPORT_HPP__
#define __DTLMOD_MAILBOX_TRANSPORT_HPP__

#include <simgrid/s4u/Mailbox.hpp>

#include "dtlmod/Transport.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class MailboxTransport : public Transport {
  std::unordered_map<std::string, sg4::Mailbox*> mboxes_;

protected:
  void add_publisher(unsigned int publisher_id) override;
  void add_subscriber(unsigned int subscriber_id) override;

public:
  MailboxTransport(const std::string& name, Engine* engine) : Transport(engine) {}
  void put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes) override;
  void get(std::shared_ptr<Variable> var) override;
};
/// \endcond

} // namespace dtlmod
#endif
