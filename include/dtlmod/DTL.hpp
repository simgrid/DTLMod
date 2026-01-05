/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_DTL_HPP__
#define __DTLMOD_DTL_HPP__

#include <optional>

#include <xbt/log.h>

#include "dtlmod/Stream.hpp"

namespace dtlmod {

/// @brief A class that implements a Data Transport Layer abstraction.
class DTL {
  friend class Stream;

  sg4::MutexPtr mutex_ = sg4::Mutex::create();
  std::set<simgrid::s4u::Actor*> active_connections_;
  std::unordered_map<std::string, std::shared_ptr<Stream>> streams_;

  void connection_manager_connect(simgrid::s4u::Actor* actor);
  void connection_manager_disconnect(simgrid::s4u::Actor* actor);
  static void connection_manager_init(std::shared_ptr<DTL> dtl);

protected:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  void lock() { mutex_->lock(); }
  void unlock() { mutex_->unlock(); }
  /// \endcond

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  explicit DTL(const std::string& filename);
  /// \endcond

  /// @brief Create the Data Transport Layer.
  /// @param filename: a JSON configuration file that provide stream parameters.
  static void create(const std::string& filename = "");

  /// @brief Connect an Actor to the Data Transport Layer.
  /// @return A handler on the DTL object.
  [[nodiscard]] static std::shared_ptr<DTL> connect();
  /// @brief Disconnect an Actor from the Data Transport Layer.
  static void disconnect();

  /// @brief Helper function to check whether some simulated actors are currently connected to the DTL.
  /// @return A boolean value.
  bool has_active_connections() const { return !active_connections_.empty(); }

  /// @brief Add a data stream to the Data Transport Layer.
  /// @param name The name of the Stream to add to the DTL.
  /// @return A handler on the newly created Stream object.
  [[nodiscard]] std::shared_ptr<Stream> add_stream(const std::string& name);

  /// @brief Retrieve all streams declared in the Data Transport Layer.
  /// @return a map of handlers on Stream objects with their names as keys.
  const std::unordered_map<std::string, std::shared_ptr<Stream>>& get_all_streams() const { return streams_; }

  /// @brief Retrieve a data stream from the Data Transport Layer by its name.
  /// @param name The name of the Stream to retrieve.
  /// @return An optional containing the Stream handler if found, std::nullopt otherwise.
  [[nodiscard]] std::optional<std::shared_ptr<Stream>> get_stream_by_name(const std::string& name) const;
};

} // namespace dtlmod
#endif
