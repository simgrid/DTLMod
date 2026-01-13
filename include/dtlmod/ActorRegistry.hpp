/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_ACTOR_REGISTRY_HPP__
#define __DTLMOD_ACTOR_REGISTRY_HPP__

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Barrier.hpp>
#include <simgrid/s4u/Mutex.hpp>
#include <xbt/asserts.h>

#include <limits>
#include <set>

namespace sg4 = simgrid::s4u;

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
/// @brief A class that manages a registry of actors (publishers or subscribers) for an Engine.
/// This class encapsulates actor management logic including addition, removal, lookup,
/// and synchronization via barriers.

class ActorRegistry {
  friend class Engine;
  sg4::MutexPtr mutex_ = sg4::Mutex::create();
  std::set<sg4::ActorPtr> actors_;
  sg4::BarrierPtr barrier_ = nullptr;

public:
  ActorRegistry() = default;

  void add(sg4::ActorPtr actor)
  {
    xbt_assert(actor != nullptr, "Cannot add null actor to registry");
    actors_.insert(actor);
  }

  void remove(sg4::ActorPtr actor)
  {
    xbt_assert(actor != nullptr, "Cannot remove null actor from registry");
    actors_.erase(actor);
  }

  [[nodiscard]] bool contains(sg4::ActorPtr actor) const noexcept
  {
    if (!actor)
      return false; // Safe handling for noexcept
    return actors_.find(actor) != actors_.end();
  }

  [[nodiscard]] size_t count() const noexcept { return actors_.size(); }
  [[nodiscard]] const std::set<sg4::ActorPtr>& get_actors() const noexcept { return actors_; }
  [[nodiscard]] bool is_empty() const noexcept { return actors_.empty(); }
  [[nodiscard]] sg4::BarrierPtr get_or_create_barrier()
  {
    if (!barrier_) {
      // Assume that nobody will create a system with more than UINT_MAX actors
      barrier_ = sg4::Barrier::create(static_cast<unsigned int>(actors_.size()));
    }
    return barrier_;
  }
  [[nodiscard]] bool is_last_at_barrier() { return barrier_ && barrier_->wait(); }
  [[nodiscard]] sg4::MutexPtr get_mutex() noexcept { return mutex_; };
};

} // namespace dtlmod
#endif
