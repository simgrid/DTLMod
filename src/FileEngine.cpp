/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fsmod/FileSystem.hpp>
#include <fsmod/PathUtil.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/FileEngine.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_file_engine, dtlmod_engine, "DTL logging about file-based Engines");

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

// FileEngines require to know where to (virtually) write file. This information is given by fullpath which has the
// following format: NetZone:FileSystem:PathToDirectory
FileEngine::FileEngine(const std::string& fullpath, const std::shared_ptr<Stream>& stream)
    : Engine(fullpath, stream, Engine::Type::File)
{
  XBT_DEBUG("Create a new FileEngine writing in %s", fullpath.c_str());

  // Parse fullpath
  std::vector<std::string> tokens;
  boost::split(tokens, fullpath, boost::is_any_of(":"), boost::token_compress_on);
  if (tokens.size() != 3)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, fullpath);

  // Get the NetZone first
  netzone_ = sg4::Engine::get_instance()->netzone_by_name_or_null(tokens[0]);
  if (!netzone_)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, "Unknown NetZone named: " + tokens[0]);

  // Get the file system in this NetZone. If no file system with the given name exists, the .at() raises an exception
  try {
    file_system_ = sgfs::FileSystem::get_file_systems_by_netzone(netzone_).at(tokens[1]);
  } catch (const std::out_of_range&) {
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, "Unknown File System named: " + tokens[1]);
  }
  // Extract the partition from the PathToDirectory
  partition_ = file_system_->get_partition_for_path_or_null(tokens[2]);
  if (!partition_)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, "Cannot find a partition for that name: " + tokens[2]);

  // Parse the path within the partition for debug purposes
  auto simplified_path                   = sgfs::PathUtil::simplify_path_string(tokens[2]);
  auto path_at_mp                        = sgfs::PathUtil::path_at_mount_point(simplified_path, partition_->get_name());
  std::tie(working_directory_, dataset_) = sgfs::PathUtil::split_path(path_at_mp);
  XBT_DEBUG("Partition: %s; Working Directory : %s; Data Set: %s", partition_->get_cname(), working_directory_.c_str(),
            dataset_.c_str());

  // If this directory doesn't exist yet, create it
  if (!file_system_->directory_exists(tokens[2])) {
    XBT_DEBUG("Creating Directory '%s' on '%s' partition", path_at_mp.c_str(), partition_->get_cname());
    file_system_->create_directory(tokens[2]);
  }
}

void FileEngine::create_transport(const Transport::Method& transport_method)
{
  // This function is called in a critical section. Transport can only be created once.
  set_transport(std::make_shared<FileTransport>(this));
}

std::string FileEngine::get_path_to_dataset() const
{
  return partition_->get_name() + working_directory_ + "/" + dataset_ + "/";
}

void FileEngine::begin_pub_transaction()
{
  auto self      = sg4::Actor::self();
  auto transport = std::static_pointer_cast<FileTransport>(transport_);

  if (!pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    current_pub_transaction_id_++;
    XBT_DEBUG("Publish Transaction %u started by %s", current_pub_transaction_id_, sg4::Actor::self()->get_cname());
  }

  if (current_pub_transaction_id_ > 1) { // This is not the first transaction.
    // Wait for the completion of the Publish activities from the previous transaction
    XBT_DEBUG("Wait for the completion of %u publish activities from the previous transaction",
              file_pub_transaction_[self].size());
    while (file_pub_transaction_[self].size() > 0)
      pub_activities_completed_->wait(std::unique_lock(*pub_mutex_));
    XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
    transport->clear_to_write_in_transaction(self);
  }
}

void FileEngine::end_pub_transaction()
{
  auto self      = sg4::Actor::self();
  auto transport = std::static_pointer_cast<FileTransport>(transport_);

  // This is the end of the first transaction, create a barrier
  if (!pub_barrier_) {
    XBT_DEBUG("Create a barrier for %zu publishers", publishers_.size());
    pub_barrier_ = sg4::Barrier::create(publishers_.size());
  }

  // Publisher gets the list of files and size to write that has been build during the put() operations
  auto to_write = transport->get_to_write_in_transaction_by_actor(self);

  // Start the write activities for that transaction
  XBT_DEBUG("Start the %d publish activities for the transaction", file_pub_transaction_[self].size());
  for (const auto& [file, size] : to_write) {
    auto write = file->write_async(size, true);
    write->on_this_completion_cb([this, self, write](sg4::Io const&) {
      pub_activities_completed_->notify_all();
      file_pub_transaction_[self].erase(write);
    });
    file_pub_transaction_[self].push(write);
  }

  if (is_last_publisher()) {
    // Mark this transaction as over
    pub_transaction_in_progress_ = false;
    // A new pub transaction has been completed, notify subscribers
    XBT_DEBUG("Notify subscribers that transaction %u is over", completed_pub_transaction_id_);
    completed_pub_transaction_id_++;
    pub_transaction_completed_->notify_all();
  }
}

void FileEngine::pub_close()
{
  auto self      = sg4::Actor::self();
  auto transport = std::static_pointer_cast<FileTransport>(transport_);

  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());

  XBT_DEBUG("[%s] Wait for the completion of %u publish activities from the previous transaction", get_cname(),
            file_pub_transaction_[self].size());
  while (file_pub_transaction_[self].size() > 0)
    pub_activities_completed_->wait(std::unique_lock(*pub_mutex_));
  transport->clear_to_write_in_transaction(self);

  rm_publisher(self);

  // Synchronize Publishers on engine closing
  if (is_last_publisher()) {
    XBT_DEBUG("[%s] last publish transaction is over", get_cname());
    XBT_DEBUG("All publishers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Closing opened files");
    transport->close_pub_files();
    XBT_DEBUG("Engine '%s' is now closed for all publishers ", get_cname());
    if (stream_.lock()->does_export_metadata())
      export_metadata_to_file();
  }
}

void FileEngine::begin_sub_transaction()
{
  // Only one subscriber has to do this
  if (!sub_transaction_in_progress_) {
    sub_transaction_in_progress_ = true;
    current_sub_transaction_id_++;
    XBT_DEBUG("Subscribe Transaction %u started by %s", current_sub_transaction_id_, sg4::Actor::self()->get_cname());
  }

  // We have publishers on that stream, wait for them to complete a transaction first
  if (get_num_publishers() > 0) {
    std::unique_lock lock(*sub_mutex_);
    while (completed_pub_transaction_id_ < current_sub_transaction_id_) {
      XBT_DEBUG("Wait for publishers to end the transaction I need");
      pub_transaction_completed_->wait(lock);
    }
    XBT_DEBUG("Publishers stored metadata for that transaction, proceed");
  }
}

void FileEngine::end_sub_transaction()
{
  auto self      = sg4::Actor::self();
  auto transport = std::static_pointer_cast<FileTransport>(transport_);

  // The files subscribers need to read may not have been fully written. Wait to be notified completion of the publish
  // activities
  if (current_sub_transaction_id_ == current_pub_transaction_id_ && get_num_publishers() > 0) {
    XBT_DEBUG("Wait for the completion of publish activities from the current transaction");
    pub_activities_completed_->wait(std::unique_lock(*sub_mutex_));
    XBT_DEBUG("All on-flight publish activities are completed. Proceed with the subscribe activities.");
  }

  // Subscriber get the list of files and size to read that has been build during the get() operations
  auto to_read = transport->get_to_read_in_transaction_by_actor(self);

  // Start the read activities for that transaction
  for (const auto& [file, size] : to_read)
    file_sub_transaction_[self].push(file->read_async(size));

  XBT_DEBUG("Wait for the %d subscribe activities for the transaction", file_sub_transaction_[self].size());
  file_sub_transaction_[self].wait_all();
  file_sub_transaction_[self].clear();
  // Close files opened in this transaction
  transport->close_sub_files(self);
  // Clear the vector of things to read
  transport->clear_to_read_in_transaction(self);

  XBT_DEBUG("All on-flight subscribe activities are completed.");
  // Mark this transaction as over
  sub_transaction_in_progress_ = false;
}

void FileEngine::sub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());

  rm_subscriber(self);
  // Synchronize subscribers on engine closing
  if (is_last_subscriber()) {
    XBT_DEBUG("All subscribers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Engine '%s' is now closed for all subscribers ", get_cname());
  }
}

/// \endcond
} // namespace dtlmod
