/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

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
    /// @brief The Engine Type has not nee specified yet.
    Undefined,
    /// @brief File Engine. Relies on files written to and read from a file system to transport data from publisher(s)
    /// to subscriber(s).
    File,
    /// @brief Staging Engine. Relies on communications to to transport data from publisher(s) to subscriber(s).
    Staging
  };

private:
  friend class Stream;
  friend class FileTransport;
  friend class MailboxTransport;
  friend class MessageQueueTransport;

  std::string name_;
  Type type_                            = Type::Undefined;
  std::shared_ptr<Transport> transport_ = nullptr;
  Stream* stream_;
  sg4::ConditionVariablePtr open_sync_;
  std::set<sg4::ActorPtr> publishers_;
  sg4::BarrierPtr pub_barrier_ = nullptr;
  sg4::ActivitySet pub_transaction_;
  sg4::MutexPtr pub_mutex_;
  unsigned int pub_transaction_id_  = 0;
  bool pub_transaction_in_progress_ = false;
  bool pub_closing_                 = false;
  void begin_pub_transaction();
  void end_pub_transaction();
  void pub_close();

  std::set<sg4::ActorPtr> subscribers_;
  sg4::BarrierPtr sub_barrier_ = nullptr;
  sg4::ActivitySet sub_transaction_;
  sg4::MutexPtr sub_mutex_;
  sg4::ConditionVariablePtr first_pub_transaction_started_;
  sg4::ConditionVariablePtr sub_transaction_started_;
  sg4::ConditionVariablePtr pub_transaction_completed_;

  unsigned int sub_transaction_id_  = 1;
  bool sub_transaction_in_progress_ = false;
  bool sub_closing_                 = false;
  void begin_sub_transaction();
  void end_sub_transaction();
  void sub_close();
  void export_metadata_to_file();

protected:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  Stream* get_stream() const { return stream_; }

  virtual void create_transport(const Transport::Method& transport_method) = 0;

  void set_transport(std::shared_ptr<Transport> transport) { transport_ = transport; }
  [[nodiscard]] std::shared_ptr<Transport> get_transport() const { return transport_; }

  void add_publisher(sg4::ActorPtr actor, bool rendez_vous);
  void rm_publisher(sg4::ActorPtr actor) { publishers_.erase(actor); }
  void add_publish_activity(sg4::ActivityPtr a) { pub_transaction_.push(a); }
  [[nodiscard]] const std::set<sg4::ActorPtr>& get_publishers() const { return publishers_; }
  [[nodiscard]] size_t get_num_publishers() const { return publishers_.size(); }
  [[nodiscard]] bool is_publisher(sg4::ActorPtr actor) const { return publishers_.find(actor) != publishers_.end(); }

  void add_subscriber(sg4::ActorPtr actor, bool rendez_vous);
  void add_subscribe_activity(sg4::ActivityPtr a) { sub_transaction_.push(a); }
  void rm_subscriber(sg4::ActorPtr actor) { subscribers_.erase(actor); }
  [[nodiscard]] size_t get_num_subscribers() const { return subscribers_.size(); }
  [[nodiscard]] bool is_subscriber(sg4::ActorPtr actor) const { return subscribers_.find(actor) != subscribers_.end(); }
  /// \endcond

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  explicit Engine(const std::string& name, Stream* stream, Type type)
      : name_(name)
      , type_(type)
      , stream_(stream)
      , open_sync_(sg4::ConditionVariable::create())
      , pub_mutex_(sg4::Mutex::create())
      , sub_mutex_(sg4::Mutex::create())
      , first_pub_transaction_started_(sg4::ConditionVariable::create())
      , sub_transaction_started_(sg4::ConditionVariable::create())
      , pub_transaction_completed_(sg4::ConditionVariable::create())
  {
  }
  virtual ~Engine() = default;
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
  /// @param simulated_size_in_bytes The size of the (subset of) the Variable
  void put(std::shared_ptr<Variable> var, size_t simulated_size_in_bytes);

  /// @brief Get a Variable from the DTL
  /// @param var The Variable to get in the DTL (Have to do an Inquire first).
  void get(std::shared_ptr<Variable> var);

  /// @brief End a transaction on an Engine.
  void end_transaction();

  /// @brief Get the id of the current transaction (on the Publish side).
  /// @return The id of the ongoing transaction.
  [[nodiscard]] unsigned int get_current_transaction() const { return pub_transaction_id_; }

  /// @brief Close the Engine associated to a Stream.
  void close();
};

} // namespace dtlmod
#endif
