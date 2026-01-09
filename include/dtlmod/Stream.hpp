/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_STREAM_HPP__
#define __DTLMOD_STREAM_HPP__

#include <optional>
#include <simgrid/s4u/ConditionVariable.hpp>

#include "dtlmod/Engine.hpp"

namespace dtlmod {

class DTL;

/** @brief A class that implements the Stream abstraction in the DTL. A Stream defines the connection between the
 *         applications that produce or consume data and the DTL
 *
 *         A Stream acts as a factory of Engine objects which are in charge of moving or storing data according
 *         to a specified Transport method. Opening a Stream creates and returns a handler on an Engine.
 *
 *         A Stream also acts as a factory of Variable objects. It keep records of the definition of the Variable
 *         objects to be injected into or retrieve from this Stream.
 */

class Stream : public std::enable_shared_from_this<Stream> {
  friend class Engine;
  friend class FileEngine;
  friend class StagingEngine;

public:
  /// @brief An enum that defines the access mode for a Stream
  enum class Mode {
    /// @brief Publish. To inject data into the DTL.
    Publish = 0,
    /// @brief Subscribe. To retrieve data from the DTL.
    Subscribe = 1
  };

private:
  const std::string name_;
  DTL* dtl_                           = nullptr;
  std::shared_ptr<Engine> engine_     = nullptr;
  Engine::Type engine_type_           = Engine::Type::Undefined;
  Transport::Method transport_method_ = Transport::Method::Undefined;
  bool metadata_export_               = false;
  std::string metadata_file_;
  sg4::MutexPtr mutex_                = sg4::Mutex::create();
  sg4::ConditionVariablePtr engine_created_ = sg4::ConditionVariable::create();
  Mode access_mode_;

  std::unordered_map<std::string, std::shared_ptr<Variable>> variables_;

protected:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  [[nodiscard]] const Transport::Method& get_transport_method() const noexcept { return transport_method_; }

  [[nodiscard]] const char* mode_to_str(Mode mode) const noexcept
  {
    return (mode == Mode::Publish) ? "Mode::Publish" : "Mode::Subscribe";
  }
  void close() noexcept { engine_ = nullptr; }

  void export_metadata_to_file() const;

  // Helper methods for Stream::open
  void validate_open_parameters(std::string_view name, Mode mode) const;
  void create_engine_if_needed(std::string_view name, Mode mode);
  void wait_for_engine_creation();
  void register_actor_with_engine(Mode mode) const;
  /// \endcond

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  Stream(const std::string& name, DTL* dtl) : name_(name), dtl_(dtl) {}
  Stream(const Stream&)            = delete;
  Stream& operator=(const Stream&) = delete;
  // Move operations are deleted because:
  // 1. Stream inherits from std::enable_shared_from_this, making move semantics problematic
  // 2. Contains non-owning raw pointer (dtl_) and synchronization primitives (mutex_, condition variables)
  // 3. Has bidirectional relationships with Engine objects that hold weak_ptr<Stream>
  // 4. Always managed via std::shared_ptr, so move operations are never needed
  Stream(Stream&&)                 = delete;
  Stream& operator=(Stream&&)      = delete;
  ~Stream() noexcept               = default;
  /// \endcond

  /// @brief Helper function to print out the name of the Stream.
  /// @return The corresponding string
  [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
  /// @brief Helper function to print out the name of the Stream.
  /// @return The corresponding C-string
  [[nodiscard]] const char* get_cname() const noexcept { return name_.c_str(); }
  /// @brief Helper function to print out the Engine::Type of the Stream.
  /// @return An optional containing the C-string if the type is valid, std::nullopt otherwise
  [[nodiscard]] std::optional<const char*> get_engine_type_str() const noexcept;
  /// @brief Helper function to print out the Transport::Method of the Stream.
  /// @return An optional containing the C-string if the method is valid, std::nullopt otherwise
  [[nodiscard]] std::optional<const char*> get_transport_method_str() const noexcept;
  /// @brief Helper function to know the access Mode of the Stream.
  /// @return The corresponding Stream::Mode
  [[nodiscard]] Mode get_access_mode() const noexcept { return access_mode_; }
  /// @brief Helper function to print out the access Mode of the Stream.
  /// @return The corresponding C-string
  [[nodiscard]] const char* get_access_mode_str() const noexcept { return mode_to_str(access_mode_); }
  /// @brief Helper function to know if the Stream does export metadata or not
  /// @return a boolean indicating if the Stream does export metadata or not
  [[nodiscard]] bool does_export_metadata() const noexcept { return metadata_export_; }

  /// @brief Stream configuration function: set the Engine type to create.
  /// @param engine_type The type of Engine to create when opening the Stream.
  /// @return The calling Stream (enable method chaining).
  Stream& set_engine_type(const Engine::Type& engine_type);
  /// @brief Stream configuration function: set the Transport Method to use.
  /// @param transport_method the Transport methode to use when opening the Stream.
  /// @return The calling Stream (enable method chaining).
  Stream& set_transport_method(const Transport::Method& transport_method);
  /// @brief Stream configuration function: specify that metadata must be exported
  /// @return The calling Stream (enable method chaining).
  Stream& set_metadata_export() noexcept;
  /// @brief Stream configuration function: specify that metadata must not be exported
  /// @return The calling Stream (enable method chaining).
  Stream& unset_metadata_export() noexcept;
  /// @brief Get the name of the file in which the stream stores metadata
  /// @return The name of the file.
  [[nodiscard]] const std::string& get_metadata_file_name() const noexcept { return metadata_file_; }

  /******* Engine Factory *******/

  /// @brief Open a Stream and create an Engine.
  /// @param name name of the Engine created when opening the Stream.
  /// @param mode either Stream::Mode::Publish or Stream::Mode::Subscribe.
  /// @return A shared pointer on the corresponding Engine.
  [[nodiscard]] std::shared_ptr<Engine> open(std::string_view name, Mode mode);

  /// @brief Helper function to obtain the number of actors connected to Stream in Mode::Publish.
  /// @return The number of publishers for that Stream.
  [[nodiscard]] size_t get_num_publishers() const { return engine_->get_publishers().count(); }
  /// @brief Helper function to obtain the number of actors connected to Stream in Mode::Subscribe.
  /// @return The number of subscribers for that Stream.
  [[nodiscard]] size_t get_num_subscribers() const { return engine_->get_subscribers().count(); }

  /******* Variable Factory *******/

  /// @brief Define a scalar Variable for this Stream.
  /// @param name The name of the new Variable.
  /// @param element_size The size of the elements in the Variable.
  /// @return A shared pointer on the newly created Variable.
  [[nodiscard]] std::shared_ptr<Variable> define_variable(std::string_view name, size_t element_size);

  /// @brief Define a Variable for this Stream.
  /// @param name The name of the new variable.
  /// @param shape A vector that specifies the total number of element in each dimension.
  /// @param start A vector that specifies the offset at which the calling Actor start to own data in each dimension.
  /// @param count A vector that specifies how many elements the calling Actor owns in each dimension.
  /// @param element_size The size of the elements in the Variable.
  /// @return A shared pointer on the newly created Variable
  [[nodiscard]] std::shared_ptr<Variable> define_variable(std::string_view name, const std::vector<size_t>& shape,
                                                          const std::vector<size_t>& start,
                                                          const std::vector<size_t>& count, size_t element_size);

  /// @brief Retrieve the list of Variables defined on this stream
  /// @return the list of Variable names
  [[nodiscard]] std::vector<std::string> get_all_variables() const;

  /// @brief Retrieve a Variable information by name.
  /// @param name The name of desired Variable.
  /// @return Either a shared pointer on the Variable object if known, nullptr otherwise.
  [[nodiscard]] std::shared_ptr<Variable> inquire_variable(std::string_view name) const;

  /// @brief Remove a Variable of the list of variables known by the Stream.
  /// @param name The name of the variable to remove.
  void remove_variable(std::string_view name);
};

} // namespace dtlmod
#endif
