/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_STAGING_MBOX_TRANSPORT_HPP__
#define __DTLMOD_STAGING_MBOX_TRANSPORT_HPP__

#include <simgrid/s4u/Mailbox.hpp>

#include "dtlmod/StagingTransport.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class StagingMboxTransport : public StagingTransport {
  std::unordered_map<std::string, sg4::Mailbox*> mboxes_;

protected:
  void add_publisher(unsigned int publisher_id) override;
  void create_rendez_vous_points() override;
  void get_requests_and_do_put(sg4::ActorPtr publisher) override;
public:
  StagingMboxTransport(const std::string& name, Engine* engine) : StagingTransport(engine) {}
  void put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes) override;
  void get(std::shared_ptr<Variable> var) override;
};
/// \endcond

} // namespace dtlmod
#endif
