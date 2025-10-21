/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__
#define __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__

#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/Variable.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class ParametrizedCompression {
  friend class CompressionReductionMethod;
  double accuracy_;
  double compression_cost_per_element_;
  double decompression_cost_per_element_;

protected:
  ParametrizedCompression(double accuracy, double compression_cost_per_element, double decompression_cost_per_element)
    : accuracy_(accuracy)
    , compression_cost_per_element_(compression_cost_per_element)
    , decompression_cost_per_element_(decompression_cost_per_element)
  {}
};


class CompressionReductionMethod : public ReductionMethod{
  std::map<std::shared_ptr<Variable>, ParametrizedCompression> per_variable_parametrizations_;

public:
  CompressionReductionMethod(const std::string& name) : ReductionMethod(name) {}
  void parse_parameters(std::shared_ptr<Variable> var, const std::map<std::string, std::string>& parameters) override
  {
    double accuracy;
    double compression_cost_per_element;
    double decompression_cost_per_element;

    for (const auto& [key, value] : parameters) {
      if (key == "accuracy") {
        accuracy = std::stof(value);
      } else if (key == "compression_cost_per_element") {
        compression_cost_per_element = std::stof(value);
      } else if (key == "decompression_cost_per_element") {
        decompression_cost_per_element = std::stof(value);
      } // else
        // TODO handle invalid key
    }
    per_variable_parametrizations_.try_emplace(var, ParametrizedCompression(accuracy, compression_cost_per_element, decompression_cost_per_element));
  }

};
/// \endcond
} // namespace dtlmod
#endif //__DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__