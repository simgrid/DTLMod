/* Copyright (c) 2023-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ENGINE_STAGING_HPP__
#define __DTLMOD_ENGINE_STAGING_HPP__

#include "dtlmod/Engine.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class StagingEngine : public Engine {
  sg4::ConditionVariablePtr first_pub_transaction_started_;
  sg4::ConditionVariablePtr sub_transaction_started_;
  int num_subscribers_starting_ = 0;
  bool pub_closing_             = false;
  bool sub_closing_             = false;

protected:
  void begin_pub_transaction() override;
  void end_pub_transaction() override;
  void pub_close() override;
  void begin_sub_transaction() override;
  void end_sub_transaction() override;
  void sub_close() override;

public:
  explicit StagingEngine(const std::string& name, Stream* stream)
      : Engine(name, stream, Engine::Type::Staging)
      , first_pub_transaction_started_(sg4::ConditionVariable::create())
      , sub_transaction_started_(sg4::ConditionVariable::create())
  {
  }

  void create_transport(const Transport::Method& transport_method);
};
/// \endcond

} // namespace dtlmod
#endif
