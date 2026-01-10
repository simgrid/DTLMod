/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__
#define __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__

#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/Variable.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION
class ParameterizedCompression {
  friend class CompressionReductionMethod;
  double accuracy_;
  double compression_cost_per_element_;
  double decompression_cost_per_element_;

public:
  ParameterizedCompression(double accuracy, double compression_cost_per_element, double decompression_cost_per_element)
      : accuracy_(accuracy)
      , compression_cost_per_element_(compression_cost_per_element)
      , decompression_cost_per_element_(decompression_cost_per_element)
  {
  }
};

class CompressionReductionMethod : public ReductionMethod {
  std::map<std::shared_ptr<Variable>, std::shared_ptr<ParameterizedCompression>> per_variable_parameterizations_;

public:
  CompressionReductionMethod(const std::string& name) : ReductionMethod(name) {}
  void parameterize_for_variable(std::shared_ptr<Variable> var,
                                 const std::map<std::string, std::string>& parameters) override
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
    per_variable_parameterizations_.try_emplace(
        var, std::make_shared<ParameterizedCompression>(accuracy, compression_cost_per_element,
                                                        decompression_cost_per_element));
  }
  void reduce_variable(std::shared_ptr<Variable> /* var*/) override {}
  [[nodiscard]] size_t get_reduced_variable_global_size(std::shared_ptr<Variable> /*var*/) const override { return 0; }
  [[nodiscard]] size_t get_reduced_variable_local_size(std::shared_ptr<Variable> /*var*/) const override { return 0; }
  [[nodiscard]] const std::vector<size_t>& get_reduced_variable_shape(std::shared_ptr<Variable> var) const override
  {
    return var->get_shape();
  }
  [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(std::shared_ptr<Variable> /*var*/, sg4::ActorPtr /*publisher*/) const override
  {
    throw std::runtime_error("not implemented");
    // return;// std::make_pair(std::vector<size_t>(), std::vector<size_t>());
  }
  [[nodiscard]] double get_flop_amount_to_reduce_variable(std::shared_ptr<Variable> /*var*/) const override
  {
    return 0.0;
  }
};
/// \endcond
} // namespace dtlmod
#endif //__DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__