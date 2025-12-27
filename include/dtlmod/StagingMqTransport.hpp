/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_STAGING_MQ_TRANSPORT_HPP__
#define __DTLMOD_STAGING_MQ_TRANSPORT_HPP__

#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/Engine.hpp"
#include "dtlmod/StagingTransport.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class StagingMqTransport : public StagingTransport {
  using StagingTransport::StagingTransport;
  std::unordered_map<std::string, sg4::MessageQueue*> mqueues_;

protected:
  void create_rendez_vous_points() override;
  void get_requests_and_do_put(sg4::ActorPtr publisher) override;
  void get_rendez_vous_point_and_do_get(const std::string& name) override;
};
/// \endcond

} // namespace dtlmod
#endif
