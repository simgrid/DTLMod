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

protected:
  std::string name_;
  Type type_                            = Type::Undefined;
  std::shared_ptr<Transport> transport_ = nullptr;
  std::weak_ptr<Stream> stream_;

  sg4::MutexPtr pub_mutex_ = sg4::Mutex::create();
  std::set<sg4::ActorPtr> publishers_;
  sg4::ActivitySet pub_transaction_;
  sg4::ConditionVariablePtr pub_transaction_completed_ = sg4::ConditionVariable::create();
  sg4::BarrierPtr pub_barrier_                         = nullptr;
  unsigned int current_pub_transaction_id_             = 0;
  unsigned int completed_pub_transaction_id_           = 0;
  bool pub_transaction_in_progress_                    = false;
  virtual void begin_pub_transaction()                 = 0;
  virtual void end_pub_transaction()                   = 0;
  virtual void pub_close()                             = 0;

  sg4::MutexPtr sub_mutex_ = sg4::Mutex::create();
  std::set<sg4::ActorPtr> subscribers_;
  sg4::ActivitySet sub_transaction_;

  sg4::BarrierPtr sub_barrier_             = nullptr;
  unsigned int current_sub_transaction_id_ = 0;
  bool sub_transaction_in_progress_        = false;
  virtual void begin_sub_transaction()     = 0;
  virtual void end_sub_transaction()       = 0;
  virtual void sub_close()                 = 0;

  std::string metadata_file_;
  void export_metadata_to_file() const;

  /// \cond EXCLUDE_FROM_DOCUMENTATION
  void close_stream() const;

  virtual void create_transport(const Transport::Method& transport_method) = 0;

  void set_transport(std::shared_ptr<Transport> transport) { transport_ = transport; }
  [[nodiscard]] std::shared_ptr<Transport> get_transport() const { return transport_; }

  void add_publisher(sg4::ActorPtr actor);
  void rm_publisher(sg4::ActorPtr actor) { publishers_.erase(actor); }
  [[nodiscard]] bool is_publisher(sg4::ActorPtr actor) const { return publishers_.find(actor) != publishers_.end(); }
  // Synchronize publishers on engine closing
  [[nodiscard]] int is_last_publisher() const { return (pub_barrier_ && pub_barrier_->wait()); }

  void add_subscriber(sg4::ActorPtr actor);
  void rm_subscriber(sg4::ActorPtr actor) { subscribers_.erase(actor); }
  [[nodiscard]] bool is_subscriber(sg4::ActorPtr actor) const { return subscribers_.find(actor) != subscribers_.end(); }
  // Synchronize subscribers on engine closing
  [[nodiscard]] int is_last_subscriber() const { return subscribers_.empty(); }

  void set_metadata_file_name();
  /// \endcond

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  explicit Engine(const std::string& name, std::shared_ptr<Stream> stream, Type type)
      : name_(name), type_(type), stream_(stream)
  {
  }
  virtual ~Engine() = default;
  // Public accessors for Transport classes to access ActivitySets
  [[nodiscard]] sg4::ActivitySet& get_pub_transaction() { return pub_transaction_; }
  [[nodiscard]] sg4::ActivitySet& get_sub_transaction() { return sub_transaction_; }
  [[nodiscard]] const std::set<sg4::ActorPtr>& get_publishers() const { return publishers_; }
  [[nodiscard]] size_t get_num_publishers() const { return publishers_.size(); }
  [[nodiscard]] size_t get_num_subscribers() const { return subscribers_.size(); }
  /// \endcond

  /// @brief Helper function to print out the name of the Engine.
  /// @return the corresponding string
  [[nodiscard]] const std::string& get_name() const { return name_; }
  /// @brief Helper function to print out the name of the Engine.
  /// @return the corresponding C-string
  [[nodiscard]] const char* get_cname() const { return name_.c_str(); }

  /// @brief Start a transaction on an Engine.
  void begin_transaction();

  /// @brief Put a Variable in the DTL using a specific Engine.
  /// @param var The variable to put in the DTL
  void put(std::shared_ptr<Variable> var) const;

  /// @brief Put a Variable in the DTL using a specific Engine.
  /// @param var The variable to put in the DTL
  /// @param simulated_size_in_bytes The simulated size of the Variable (can be different of actual size)
  void put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes) const;

  /// @brief Get a Variable from the DTL
  /// @param var The Variable to get in the DTL (Have to do an Inquire first).
  void get(std::shared_ptr<Variable> var) const;

  /// @brief End a transaction on an Engine.
  void end_transaction();

  /// @brief Get the id of the current transaction (on the Publish side).
  /// @return The id of the ongoing transaction.
  [[nodiscard]] unsigned int get_current_transaction() const { return current_pub_transaction_id_; }

  /// @brief Get the name of the file in which the engine stored metadata
  /// @return The name of the file.
  [[nodiscard]] const std::string& get_metadata_file_name() const { return metadata_file_; }

  /// @brief Close the Engine associated to a Stream.
  void close();
};

} // namespace dtlmod
#endif
