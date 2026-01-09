/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ENGINE_FILE_HPP__
#define __DTLMOD_ENGINE_FILE_HPP__

#include <fsmod/FileSystem.hpp>
#include <fsmod/Partition.hpp>
#include <simgrid/plugins/jbod.hpp>
#include <simgrid/s4u/Disk.hpp>

#include "dtlmod/Engine.hpp"
#include "dtlmod/FileTransport.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class FileEngine : public Engine {
  friend class Stream;
  friend class FileTransport;

  sg4::NetZone* netzone_ = nullptr;
  std::shared_ptr<sgfs::FileSystem> file_system_;
  std::shared_ptr<sgfs::Partition> partition_;
  std::string working_directory_;
  std::string dataset_;
  sg4::ConditionVariablePtr pub_activities_completed_ = sg4::ConditionVariable::create();
  std::unordered_map<sg4::ActorPtr, sg4::ActivitySet> file_sub_transaction_;
  std::unordered_map<sg4::ActorPtr, sg4::ActivitySet> file_pub_transaction_;
  unsigned int current_pub_transaction_id_             = 0;
  unsigned int completed_pub_transaction_id_           = 0;
  bool pub_transaction_in_progress_                    = false;
  sg4::ConditionVariablePtr pub_transaction_completed_ = sg4::ConditionVariable::create();

  unsigned int current_sub_transaction_id_ = 0;
  bool sub_transaction_in_progress_        = false;

  void create_transport(const Transport::Method& transport_method) override;
  const std::shared_ptr<sgfs::FileSystem>& get_file_system() const { return file_system_; }
  std::string get_path_to_dataset() const;
  void begin_pub_transaction() override;
  void end_pub_transaction() override;
  void pub_close() override;
  void begin_sub_transaction() override;
  void end_sub_transaction() override;
  void sub_close() override;
  [[nodiscard]] unsigned int get_current_transaction_impl() const noexcept override
  {
    return current_pub_transaction_id_;
  }

protected:
  [[nodiscard]] std::shared_ptr<FileTransport> get_file_transport() const;

public:
  explicit FileEngine(std::string_view fullpath, const std::shared_ptr<Stream>& stream);
};
/// \endcond

} // namespace dtlmod
#endif
