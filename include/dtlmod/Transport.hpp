/* Copyright (c) 2023-2024. The SWAT Team. All rights reserved.          */

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
  void set_publisher_put_requests_mq(const std::string& publisher_name);
  [[nodiscard]] sg4::MessageQueue* get_publisher_put_requests_mq(const std::string& publisher_name) const;

  virtual void add_publisher(unsigned int /* publisher_id */) { /* No-op (for now)*/ }

  virtual void add_subscriber(unsigned int /* subscriber_id */) { /* No-op (for now)*/ }
  std::vector<std::pair<std::string, sg_size_t>> check_selection_and_get_blocks_to_get(std::shared_ptr<Variable> var);

public:
  enum class Method { Undefined, File, Mailbox, MQ }; // TODO: Could add MPI?

  explicit Transport(Engine* engine) : engine_(engine) {}
  virtual ~Transport() = default;

  Engine* get_engine() { return engine_; }

  virtual void put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes) = 0;
  virtual void get(std::shared_ptr<Variable> var)                                 = 0;
};
/// \endcond

} // namespace dtlmod
#endif
