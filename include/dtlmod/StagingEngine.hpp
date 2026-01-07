/* Copyright (c) 2023-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ENGINE_STAGING_HPP__
#define __DTLMOD_ENGINE_STAGING_HPP__

#include <atomic>

#include "dtlmod/Engine.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class StagingEngine : public Engine {
  sg4::ConditionVariablePtr first_pub_transaction_started_ = sg4::ConditionVariable::create();
  sg4::ConditionVariablePtr sub_transaction_started_       = sg4::ConditionVariable::create();
  std::atomic<unsigned int> num_subscribers_starting_{0};
  bool pub_closing_                                        = false;
  bool sub_closing_                                        = false;
  unsigned int current_pub_transaction_id_                 = 0;
  unsigned int completed_pub_transaction_id_               = 0;
  bool pub_transaction_in_progress_                        = false;
  sg4::ConditionVariablePtr pub_transaction_completed_     = sg4::ConditionVariable::create();

  unsigned int current_sub_transaction_id_ = 0;
  bool sub_transaction_in_progress_        = false;

protected:
  void begin_pub_transaction() override;
  void end_pub_transaction() override;
  void pub_close() override;
  void begin_sub_transaction() override;
  void end_sub_transaction() override;
  void sub_close() override;
  [[nodiscard]] unsigned int get_current_transaction() const noexcept override { return current_pub_transaction_id_; }

public:
  explicit StagingEngine(const std::string& name, const std::shared_ptr<Stream>& stream)
      : Engine(name, stream, Engine::Type::Staging)
  {
  }

  void create_transport(const Transport::Method& transport_method) override;
};
/// \endcond

} // namespace dtlmod
#endif
