/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_REDUCTION_METHOD_HPP__
#define __DTLMOD_REDUCTION_METHOD_HPP__

#include <map>
#include <memory>
#include <string>

namespace dtlmod {

class Variable;

/// \cond EXCLUDE_FROM_DOCUMENTATION
class ReductionMethod {
  friend class DecimationReductionMethod;

  std::string name_;

public:
  ReductionMethod(const std::string& name) : name_(name){}
  virtual void parse_parameters(std::shared_ptr<Variable> var,
                                const std::map<std::string, std::string>& parameters) = 0;
  
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