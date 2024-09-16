/* Copyright (c) 2024. The SWAT Team. All rights reserved.          */

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

void FileTransport::add_publisher(unsigned int publisher_id)
{
  auto* e       = static_cast<FileEngine*>(get_engine());
  auto self     = sg4::Actor::self();
  auto filename = e->get_path_to_dataset() + "data." + std::to_string(publisher_id);
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
  // Create an I/O activity by writing 'size' bytes in 'file' and suspend it immediatly.
  // Indeed, write_async starts the I/O activity, while we want to start it (then resume it) only at the end of the
  // transaction.
  get_engine()->pub_transaction_.push(file->write_async(size)->suspend());
}

void FileTransport::get(std::shared_ptr<Variable> var)
{
  auto* e     = static_cast<FileEngine*>(get_engine());
  auto self   = sg4::Actor::self();
  auto blocks = check_selection_and_get_blocks_to_get(var);

  for (auto [filename, size] : blocks) {
    // Files are opened in read mode.
    auto file = e->get_file_system()->open(filename, "r");
    // Keep track of the files opened by subscribers for this engine to properly close them later
    subscribers_to_files_[self] = file;
    e->sub_transaction_.push(file->read_async(size)->suspend());
  }
}

void FileTransport::close_files()
{
  auto fs = static_cast<FileEngine*>(get_engine())->get_file_system();
  for (auto [actor, file] : publishers_to_files_) {
    XBT_DEBUG("Closing %s", file->get_path().c_str());
    fs->close(file);
  }
  for (auto [actor, file] : subscribers_to_files_) {
    XBT_DEBUG("Closing %s", file->get_path().c_str());
    fs->close(file);
  }
}

/// \endcond

} // namespace dtlmod
