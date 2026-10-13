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
  virtual void create_rendez_vous_points()                             = 0;
  virtual void get_rendez_vous_point_and_do_get(std::string_view name) = 0;
  // Start sending size bytes from the publisher to the subscriber and add this activity to the transaction.
  virtual void do_put(const std::string& pub_name, const std::string& sub_name, size_t size) = 0;

  // Wait for the put requests of all the subscribers and start the corresponding puts.
  void get_requests_and_do_put(sg4::ActorPtr publisher);
  // Cancel the put requests that publishers are still waiting for, or drop those of one publisher.
  void cancel_pending_put_requests();
  void drop_pending_put_requests(const std::string& pub_name);

  // Create a message queue to receive request for variable pieces from subscribers
  void set_publisher_put_requests_mq(std::string_view publisher_name);
  [[nodiscard]] sg4::MessageQueue* get_publisher_put_requests_mq(std::string_view publisher_name) const;

public:
  ~StagingTransport() override = default;
  void put(const std::shared_ptr<Variable>& var, size_t /*simulated_size_in_bytes*/) override;
  void get(const std::shared_ptr<Variable>& var) override;
};
/// \endcond

} // namespace dtlmod
#endif
