/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_FILE_TRANSPORT_HPP__
#define __DTLMOD_FILE_TRANSPORT_HPP__

#include <fsmod/File.hpp>

#include "dtlmod/Engine.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class FileTransport : public Transport {
  friend class Engine;
  friend class FileEngine;
  std::unordered_map<sg4::ActorPtr, std::shared_ptr<sgfs::File>> publishers_to_files_;
  std::unordered_map<sg4::ActorPtr, std::shared_ptr<sgfs::File>> subscribers_to_files_;

protected:
  void add_publisher(unsigned int publisher_id) override;
  void close_files();

public:
  explicit FileTransport(const std::string& name, Engine* engine) : Transport(engine) {}

  void put(std::shared_ptr<Variable> var, size_t size) override;
  void get(std::shared_ptr<Variable> var) override;
};
/// \endcond

} // namespace dtlmod
#endif
