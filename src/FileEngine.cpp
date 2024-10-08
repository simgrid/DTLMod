/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fsmod/FileSystem.hpp>
#include <fsmod/PathUtil.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/FileEngine.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

FileEngine::FileEngine(const std::string& fullpath, Stream* stream) : Engine(fullpath, stream, Engine::Type::File)
{
  XBT_DEBUG("Create a new FileEngine");
  std::vector<std::string> tokens;
  boost::split(tokens, fullpath, boost::is_any_of(":"), boost::token_compress_on);
  if (tokens.size() != 3)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, fullpath);

  netzone_ = sg4::Engine::get_instance()->netzone_by_name_or_null(tokens[0]);
  if (not netzone_)
    throw std::invalid_argument("Unknown NetZone named: " + tokens[0]);

  file_system_ = sgfs::FileSystem::get_file_systems_by_netzone(netzone_).at(tokens[1]);

  partition_ = file_system_->get_partition_for_path_or_null(tokens[2]);
  if (not partition_)
    throw std::invalid_argument("Cannot find a partition for that name: " + tokens[2]);

  auto simplified_path                   = sgfs::PathUtil::simplify_path_string(tokens[2]);
  auto path_at_mp                        = sgfs::PathUtil::path_at_mount_point(simplified_path, partition_->get_name());
  std::tie(working_directory_, dataset_) = sgfs::PathUtil::split_path(path_at_mp);
  XBT_DEBUG("Partition: %s; Working Directory : %s; Data Set: %s", partition_->get_cname(), working_directory_.c_str(),
            dataset_.c_str());

  if (not file_system_->directory_exists(tokens[2])) {
    XBT_DEBUG("Creating Directory '%s'", tokens[2].c_str());
    file_system_->create_directory(tokens[2]);
  }
}

void FileEngine::create_transport(const Transport::Method& transport_method)
{
  // This function is called in a critical section. Transport can only be created once.
  set_transport(std::make_shared<FileTransport>(get_name(), this));
}

std::string FileEngine::get_path_to_dataset() const
{
  return partition_->get_name() + working_directory_ + "/" + dataset_ + "/";
}
/// \endcond
} // namespace dtlmod
