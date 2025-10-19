/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__
#define __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__

#include "dtlmod/ReductionMethod.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class CompressionReductionMethod : public ReductionMethod{

public:
  CompressionReductionMethod(const std::string& name) : ReductionMethod(name) {}
  void parse_parameters(std::shared_ptr<Variable> var, const std::map<std::string, std::string>& parameters) override
  {
    for (const auto& [key, value] : parameters) {
      // TODO do something
    }
  }

};
/// \endcond
} // namespace dtlmod
#endif //__DTLMOD_COMPRESSIN_REDUCTION_METHOD_HPP__