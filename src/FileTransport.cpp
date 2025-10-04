/* Copyright (c) 2024-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/Actor.hpp>

#include "dtlmod/DTLException.hpp"
#include "dtlmod/FileEngine.hpp"
#include "dtlmod/FileTransport.hpp"
#include "dtlmod/Stream.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

////////////////////////////////////////////
////////////// PUBLISHER SIDE //////////////
////////////////////////////////////////////

void FileTransport::add_publisher(unsigned long publisher_id)
{
  const auto* e = static_cast<FileEngine*>(get_engine());
  auto self     = sg4::Actor::self();
  auto filename = e->get_path_to_dataset() + "data." + std::to_string(publisher_id);
  // Publishers write everything in a single file.
  XBT_DEBUG("Actor '%s' is opening file '%s'", self->get_cname(), filename.c_str());
  // Keep track of the files opened by publishers for this engine to properly close them later
  // Files are opened in 'append' mode to prevent overwritting
  publishers_to_files_[self] = e->get_file_system()->open(filename, "a");
}

void FileTransport::put(std::shared_ptr<Variable> var, size_t size)
{
  // Register who (this actor) writes in what file (the 'file' opened when adding this actor as a publisher) in this
  // transaction (the transaction_id stored by the Engine)
  auto tid  = get_engine()->get_current_transaction();
  auto self = sg4::Actor::self();
  auto file = publishers_to_files_[self];
  var->add_transaction_metadata(tid, self, file->get_path());

  XBT_DEBUG("Actor '%s' is writing %lu bytes into file '%s'", self->get_cname(), size, file->get_path().c_str());
  to_write_in_transaction_[self].emplace_back(std::make_pair(file, size));
}

void FileTransport::close_pub_files() const
{
  for (const auto& [actor, file] : publishers_to_files_) {
    XBT_DEBUG("Closing %s", file->get_path().c_str());
    file->close();
  }
}

////////////////////////////////////////////
///////////// SUBSCRIBER SIDE //////////////
////////////////////////////////////////////

void FileTransport::get(std::shared_ptr<Variable> var)
{
  auto self = sg4::Actor::self();
  auto fs   = static_cast<FileEngine*>(get_engine())->get_file_system();

  // Determine which files contain blocks of the requested (selection of) the variable
  auto blocks = check_selection_and_get_blocks_to_get(var);

  for (const auto& [filename, size] : blocks) {
    // if there is indeed something to read in this block
    if (size > 0) {
      // open the corresponding file in read mode.
      XBT_DEBUG("Actor '%s' is opening file '%s'", self->get_cname(), filename.c_str());
      auto file = fs->open(filename, "r");
      // Keep track of what to read from this file for this get
      to_read_in_transaction_[self].emplace_back(std::make_pair(file, size));
    }
  }
}

// Called at the end of a transaction. Each actor closes the files it opened in calls to get()
void FileTransport::close_sub_files(sg4::ActorPtr self)
{
  for (const auto& [file, size] : to_read_in_transaction_[self]) {
    XBT_DEBUG("Closing %s", file->get_path().c_str());
    file->close();
  }
}

/// \endcond

} // namespace dtlmod
