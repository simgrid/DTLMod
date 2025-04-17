/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ENGINE_FILE_HPP__
#define __DTLMOD_ENGINE_FILE_HPP__

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
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

protected:
  void create_transport(const Transport::Method& transport_method);
  std::shared_ptr<sgfs::FileSystem> get_file_system() const { return file_system_; }
  std::string get_path_to_dataset() const;
  void begin_pub_transaction() override;
  void pub_close() override;
  void begin_sub_transaction() override;
  void sub_close() override;

public:
  explicit FileEngine(const std::string& fullpath, Stream* stream);
};
/// \endcond

} // namespace dtlmod
#endif
