/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

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
  std::unordered_map<sg4::ActorPtr, std::vector<std::pair<std::shared_ptr<sgfs::File>, sg_size_t>>>
      to_write_in_transaction_;
  std::unordered_map<sg4::ActorPtr, std::vector<std::pair<std::shared_ptr<sgfs::File>, sg_size_t>>>
      to_read_in_transaction_;

protected:
  void add_publisher(unsigned int publisher_id) override;
  void close_pub_files();
  void close_sub_files(sg4::ActorPtr self);
  const std::vector<std::pair<std::shared_ptr<sgfs::File>, sg_size_t>>&
  get_to_write_in_transaction_by_actor(sg4::ActorPtr actor)
  {
    return to_write_in_transaction_[actor];
  }
  void clear_to_write_in_transaction(sg4::ActorPtr actor) { to_write_in_transaction_[actor].clear(); }

  const std::vector<std::pair<std::shared_ptr<sgfs::File>, sg_size_t>>&
  get_to_read_in_transaction_by_actor(sg4::ActorPtr actor)
  {
    return to_read_in_transaction_[actor];
  }
  void clear_to_read_in_transaction(sg4::ActorPtr actor) { to_read_in_transaction_[actor].clear(); }

public:
  explicit FileTransport(const std::string& name, Engine* engine) : Transport(engine) {}

  void put(std::shared_ptr<Variable> var, size_t size) override;
  void get(std::shared_ptr<Variable> var) override;
};
/// \endcond

} // namespace dtlmod
#endif
