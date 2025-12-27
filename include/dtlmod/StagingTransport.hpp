/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_STAGING_TRANSPORT_HPP__
#define __DTLMOD_STAGING_TRANSPORT_HPP__

#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Transport.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class StagingTransport : public Transport {
  using Transport::Transport;
  friend StagingEngine;
  std::unordered_map<std::string, sg4::MessageQueue*> publisher_put_requests_mq_;
  std::unordered_map<std::string, sg4::ActivitySet> pending_put_requests_;

protected:
  void add_publisher(unsigned long publisher_id) override;
  virtual void create_rendez_vous_points()                               = 0;
  virtual void get_requests_and_do_put(sg4::ActorPtr publisher)          = 0;
  virtual void get_rendez_vous_point_and_do_get(const std::string& name) = 0;

  // Create a message queue to receive request for variable pieces from subscribers
  void set_publisher_put_requests_mq(const std::string& publisher_name);
  [[nodiscard]] sg4::MessageQueue* get_publisher_put_requests_mq(const std::string& publisher_name) const;
  [[nodiscard]] bool pending_put_requests_exist_for(const std::string& pub_name)
  {
    return not pending_put_requests_[pub_name].empty();
  }
  [[nodiscard]] sg4::ActivityPtr wait_any_pending_put_request_for(const std::string& pub_name)
  {
    return pending_put_requests_[pub_name].wait_any();
  }

public:
  ~StagingTransport() override = default;
  void put(std::shared_ptr<Variable> var, size_t /*simulated_size_in_bytes*/) override;
  void get(std::shared_ptr<Variable> var) override;
};
/// \endcond

} // namespace dtlmod
#endif
