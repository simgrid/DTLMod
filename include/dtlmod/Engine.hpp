/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ENGINE_HPP__
#define __DTLMOD_ENGINE_HPP__

#include <simgrid/s4u/ActivitySet.hpp>
#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Barrier.hpp>
#include <simgrid/s4u/Comm.hpp>
#include <simgrid/s4u/ConditionVariable.hpp>
#include <simgrid/s4u/Host.hpp>
#include <simgrid/s4u/Mutex.hpp>
#include <xbt/asserts.h>
#include <xbt/log.h>

#include <string>

#include "dtlmod/ActorRegistry.hpp"
#include "dtlmod/Transport.hpp"
#include "dtlmod/Variable.hpp"

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

namespace dtlmod {

class Stream;

/// @brief A class that interface the Stream defined by users and the Transport methods that actually handle data
///        movement and storage.
class Engine {
public:
  /// @brief An enum that defines the type of Engine supported by the DTL
  enum class Type {
    /// @brief The Engine Type has not been specified yet.
    Undefined,
    /// @brief File Engine. Relies on files written to and read from a file system to transport data from publisher(s)
    /// to subscriber(s).
    File,
    /// @brief Staging Engine. Relies on communications to to transport data from publisher(s) to subscriber(s).
    Staging
  };

  friend class Stream;
  friend class StagingTransport;
  friend class StagingMboxTransport;
  friend class StagingMqTransport;

private:
  std::string name_;
  Type type_                            = Type::Undefined;
  std::shared_ptr<Transport> transport_ = nullptr;
  std::weak_ptr<Stream> stream_;

  ActorRegistry publishers_;
  ActorRegistry subscribers_;

  sg4::ActivitySet pub_transaction_;
  sg4::ActivitySet sub_transaction_;

  // Private methods for Stream (friend)
  void add_publisher(sg4::ActorPtr actor);
  void add_subscriber(sg4::ActorPtr actor);

protected:
  // Accessors for Transport classes (friend) and Python bindings
  [[nodiscard]] const sg4::ActivitySet& get_pub_transaction() const noexcept { return pub_transaction_; }
  [[nodiscard]] sg4::ActivitySet& get_pub_transaction() noexcept { return pub_transaction_; }
  [[nodiscard]] const sg4::ActivitySet& get_sub_transaction() const noexcept { return sub_transaction_; }
  [[nodiscard]] sg4::ActivitySet& get_sub_transaction() noexcept { return sub_transaction_; }

  // Protected virtual method for derived classes to implement
  [[nodiscard]] virtual unsigned int get_current_transaction_impl() const noexcept = 0;

  // Protected methods for derived classes only
  void close_stream() const;
  [[nodiscard]] std::shared_ptr<Stream> get_stream() const { return stream_.lock(); }
  void set_transport(std::shared_ptr<Transport> transport) noexcept { transport_ = transport; }
  [[nodiscard]] const std::shared_ptr<Transport>& get_transport() const noexcept { return transport_; }

  [[nodiscard]] ActorRegistry& get_publishers() noexcept { return publishers_; }
  [[nodiscard]] const ActorRegistry& get_publishers() const noexcept { return publishers_; }
  [[nodiscard]] ActorRegistry& get_subscribers() noexcept { return subscribers_; }
  [[nodiscard]] const ActorRegistry& get_subscribers() const noexcept { return subscribers_; }

  // Pure virtual methods for derived classes to implement
  virtual void create_transport(const Transport::Method& transport_method) = 0;
  virtual void begin_pub_transaction() = 0;
  virtual void end_pub_transaction()   = 0;
  virtual void pub_close()                                                 = 0;
  virtual void begin_sub_transaction() = 0;
  virtual void end_sub_transaction()   = 0;
  virtual void sub_close()             = 0;

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  explicit Engine(const std::string& name, std::shared_ptr<Stream> stream, Type type)
      : name_(name), type_(type), stream_(stream)
  {
  }
  virtual ~Engine() = default;
  /// \endcond

  /// @brief Helper function to print out the name of the Engine.
  /// @return the corresponding string
  [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
  /// @brief Helper function to print out the name of the Engine.
  /// @return the corresponding C-string
  [[nodiscard]] const char* get_cname() const noexcept { return name_.c_str(); }

  /// @brief Start a transaction on an Engine.
  void begin_transaction();

  /// @brief Put a Variable in the DTL using a specific Engine.
  /// @param var The variable to put in the DTL
  void put(const std::shared_ptr<Variable>& var) const;

  /// @brief Put a Variable in the DTL using a specific Engine.
  /// @param var The variable to put in the DTL
  /// @param simulated_size_in_bytes The simulated size of the Variable (can be different of actual size)
  void put(const std::shared_ptr<Variable>& var, size_t simulated_size_in_bytes) const;

  /// @brief Get a Variable from the DTL
  /// @param var The Variable to get in the DTL (Have to do an Inquire first).
  void get(const std::shared_ptr<Variable>& var) const;

  /// @brief End a transaction on an Engine.
  void end_transaction();

  /// @brief Get the id of the current transaction (on the Publish side).
  /// @return The id of the ongoing transaction.
  [[nodiscard]] unsigned int get_current_transaction() const noexcept { return get_current_transaction_impl(); }

  /// @brief Close the Engine associated to a Stream.
  void close();
};

} // namespace dtlmod
#endif
