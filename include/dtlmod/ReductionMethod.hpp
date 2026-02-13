/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_REDUCTION_METHOD_HPP__
#define __DTLMOD_REDUCTION_METHOD_HPP__

#include <map>
#include <string>
#include <vector>

#include <simgrid/s4u/Actor.hpp>

namespace dtlmod {

class Variable;

/// \cond EXCLUDE_FROM_DOCUMENTATION
class ReductionMethod {
  friend class Stream;
  friend class DecimationReductionMethod;
  friend class CompressionReductionMethod;

  std::string name_;

public:
  ReductionMethod(const std::string& name) : name_(name) {}
  virtual void parameterize_for_variable(const Variable& var, const std::map<std::string, std::string>& parameters) = 0;
  virtual void reduce_variable(const Variable& var)                                                                 = 0;
  virtual size_t get_reduced_variable_global_size(const Variable& var) const                                        = 0;
  virtual size_t get_reduced_variable_local_size(const Variable& var) const                                         = 0;
  virtual const std::vector<size_t>& get_reduced_variable_shape(const Variable& var) const                          = 0;
  virtual const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(const Variable& var, simgrid::s4u::ActorPtr publisher) const = 0;
  virtual double get_flop_amount_to_reduce_variable(const Variable& var) const                 = 0;

  /// @brief Helper function to print out the name of the ReductionMethod.
  /// @return The corresponding string
  [[nodiscard]] const std::string& get_name() const { return name_; }
  /// @brief Helper function to print out the name of the ReductionMethod.
  /// @return The corresponding C-string
  [[nodiscard]] const char* get_cname() const { return name_.c_str(); }
};
///\endcond
} // namespace dtlmod
#endif //__DTLMOD_REDUCTION_METHOD_HPP__
