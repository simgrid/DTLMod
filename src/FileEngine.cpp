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


// FileEngines require to know where to (virtually) write file. This information is given by fullpath which has the
// following format: NetZone:FileSystem:PathToDirectory
FileEngine::FileEngine(const std::string& fullpath, Stream* stream) : Engine(fullpath, stream, Engine::Type::File)
{
  XBT_DEBUG("Create a new FileEngine writing in %s", fullpath.c_str());

  // Parse fullpath
  std::vector<std::string> tokens;
  boost::split(tokens, fullpath, boost::is_any_of(":"), boost::token_compress_on);
  if (tokens.size() != 3)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, fullpath);

  // Get the NetZone first
  netzone_ = sg4::Engine::get_instance()->netzone_by_name_or_null(tokens[0]);
  if (not netzone_)
    throw std::invalid_argument("Unknown NetZone named: " + tokens[0]);

  // Get the file system in this NetZone. If no file system with the given name exists, the .at() raises an exception 
  file_system_ = sgfs::FileSystem::get_file_systems_by_netzone(netzone_).at(tokens[1]);

  // Extract the partition from the PathToDirectory 
  partition_ = file_system_->get_partition_for_path_or_null(tokens[2]);
  if (not partition_)
    throw std::invalid_argument("Cannot find a partition for that name: " + tokens[2]);

  // Parse the path within the partition for debug purposes
  auto simplified_path                   = sgfs::PathUtil::simplify_path_string(tokens[2]);
  auto path_at_mp                        = sgfs::PathUtil::path_at_mount_point(simplified_path, partition_->get_name());
  std::tie(working_directory_, dataset_) = sgfs::PathUtil::split_path(path_at_mp);
  XBT_DEBUG("Partition: %s; Working Directory : %s; Data Set: %s", partition_->get_cname(), working_directory_.c_str(),
            dataset_.c_str());

  // If this directory doesn't exist yet, create it 
  if (not file_system_->directory_exists(tokens[2])) {
    XBT_DEBUG("Creating Directory '%s' on '%s' partition", path_at_mp.c_str(), partition_->get_cname());
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

void FileEngine::begin_pub_transaction()
{
  // Only one publisher has to do this
  std::unique_lock<sg4::Mutex> lock(*pub_mutex_);
  if (not pub_transaction_in_progress_) {
    pub_transaction_in_progress_ = true;
    if (pub_barrier_) { // This is not the first transaction.
      // Wait for the completion of the Publish activities from the previous transaction
      XBT_DEBUG("Wait for the completion of %u publish activities from the previous transaction",
                pub_transaction_.size());
      pub_transaction_.wait_all();
      XBT_DEBUG("All on-flight publish activities are completed. Proceed with the current transaction.");
      current_pub_transaction_id_++;
      XBT_DEBUG("%u sub activities pending", sub_transaction_.size());
      if (current_pub_transaction_id_ >= sub_transaction_id_) {
        pub_transaction_.clear();
        // We may have subscribers waiting for a transaction to be over. Notify them
        pub_transaction_completed_->notify_all();
      }
    }
    XBT_DEBUG("Publish Transaction %u started by %s", current_pub_transaction_id_, sg4::Actor::self()->get_cname());
  }
}

void FileEngine::pub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Publisher '%s' is closing the engine '%s'", self->get_cname(), get_cname());
  if (not pub_closing_) {
    // I'm the first to close
    pub_closing_ = true;
    XBT_DEBUG("[%s] Wait for the completion of %u publish activities from the previous transaction", get_cname(),
              pub_transaction_.size());
    pub_transaction_.wait_all();
    pub_transaction_.clear();
    XBT_DEBUG("[%s] last publish transaction is over", get_cname());
    current_pub_transaction_id_++;
    if (get_num_subscribers() > 0 && current_pub_transaction_id_ >= sub_transaction_id_) {
        // We may have subscribers waiting for a transaction to be over. Notify them
        pub_transaction_completed_->notify_all();
      }
  }
  rm_publisher(self);

  if (is_last_publisher()) {
    XBT_DEBUG("All publishers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Closing opened files");
    std::static_pointer_cast<FileTransport>(transport_)->close_files();
    XBT_DEBUG("Engine '%s' is now closed for all publishers ", get_cname());
  }
}

void FileEngine::begin_sub_transaction()
{
  // Only one subscriber has to do this
  std::unique_lock<sg4::Mutex> lock(*sub_mutex_);

  // We have publishers on that stream, wait for them to complete a transaction first
  if (get_num_publishers() > 0) {
    while (current_pub_transaction_id_ < sub_transaction_id_)
      pub_transaction_completed_->wait(lock);
  }
  if (not sub_transaction_in_progress_) {
    XBT_DEBUG("Subscribe Transaction %u started by %s", sub_transaction_id_, sg4::Actor::self()->get_cname());
    sub_transaction_in_progress_ = true;
  }
}

void FileEngine::sub_close()
{
  auto self = sg4::Actor::self();
  XBT_DEBUG("Subscriber '%s' is closing the engine", self->get_cname());
  if (not sub_closing_) {
    // I'm the first to close
    sub_closing_ = true;
    XBT_DEBUG("Wait for the %d subscribe activities for the transaction", sub_transaction_.size());
    for (unsigned int i = 0; i < sub_transaction_.size(); i++)
      sub_transaction_.at(i)->resume()->wait();
    XBT_DEBUG("All on-flight subscribe activities are completed. Proceed with the current transaction.");
    sub_transaction_.clear();
  }
  rm_subscriber(self);

  // Synchronize subscribers on engine closing
  if (is_last_subscriber()) {
    XBT_DEBUG("All subscribers have called the Engine::close() function");
    close_stream();
    XBT_DEBUG("Closing opened files");
    std::static_pointer_cast<FileTransport>(transport_)->close_files();
    XBT_DEBUG("Engine '%s' is now closed for all subscribers ", get_cname());
  }
}

/// \endcond
} // namespace dtlmod
