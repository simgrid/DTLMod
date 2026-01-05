/* Copyright (c) 2023-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_TRANSPORT_HPP__
#define __DTLMOD_TRANSPORT_HPP__

#include "dtlmod/Variable.hpp"

#include <string>

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

class Engine;

/// \cond EXCLUDE_FROM_DOCUMENTATION
class Transport {
  friend class Engine;
  Engine* engine_;
  std::unordered_map<std::string, sg4::MessageQueue*> publisher_put_requests_mq_;

protected:
  virtual void add_publisher(unsigned long /* publisher_id */) { /* No-op (for now)*/ }

  virtual void add_subscriber(unsigned long /* subscriber_id */) { /* No-op (for now)*/ }
  std::vector<std::pair<std::string, sg_size_t>>
  check_selection_and_get_blocks_to_get(std::shared_ptr<Variable> var) const;

public:
  enum class Method { Undefined, File, Mailbox, MQ };

  explicit Transport(Engine* engine) noexcept : engine_(engine) {}
  virtual ~Transport() = default;

  Engine* get_engine() noexcept { return engine_; }

  virtual void put(const std::shared_ptr<Variable>& var, size_t simulated_size_in_bytes) = 0;
  virtual void get(const std::shared_ptr<Variable>& var)                                 = 0;
};
/// \endcond

} // namespace dtlmod
#endif
