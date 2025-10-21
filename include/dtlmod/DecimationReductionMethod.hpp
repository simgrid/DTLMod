/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__
#define __DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cmath>

#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/Variable.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION

class ParametrizedDecimation {
  friend class DecimationReductionMethod;
  std::vector<size_t> stride_;
  std::string interpolation_method_ = "";
  double cost_per_element_;

  std::vector<size_t> reduced_shape_;

protected:
  const std::vector<size_t>& get_stride() const { return stride_; }

  void set_reduced_shape(const std::vector<size_t>& reduced_shape) { reduced_shape_ = reduced_shape; }

public:
  ParametrizedDecimation(const std::vector<size_t> stride, const std::string interpolation_method,
                         double cost_per_element)
    : stride_(stride), interpolation_method_(interpolation_method), cost_per_element_(cost_per_element) {}

};


class DecimationReductionMethod : public ReductionMethod{
  std::map<std::shared_ptr<Variable>, std::shared_ptr<ParametrizedDecimation>> per_variable_parametrizations_;

public:
  DecimationReductionMethod(const std::string& name) : ReductionMethod(name) {}

  void parametrize_for_variable(std::shared_ptr<Variable> var, const std::map<std::string, std::string>& parameters) override
  {
    std::vector<size_t> stride;
    std::string interpolation_method = "";
    double cost_per_element          = 1.0;

    for (const auto& [key, value] : parameters) {
      if (key == "stride") {
        std::vector<std::string> tokens;
        boost::split(tokens, value, boost::is_any_of(","), boost::token_compress_on);
        for (const auto t : tokens)
          stride.push_back(std::stoul(t));
      } else if (key == "interpolation") {
        interpolation_method = value;
      } else if (key == "cost_per_element") {
        cost_per_element = std::stod(value);
      } // else
        // TODO handle invalid key
    }
    per_variable_parametrizations_.try_emplace(var, std::make_shared<ParametrizedDecimation>(stride, interpolation_method, cost_per_element));
  }

  void reduce_variable(std::shared_ptr<Variable> var) 
  {
    auto parametrization = per_variable_parametrizations_[var];
    auto stride = parametrization->get_stride();
    
    std::vector<size_t> reduced_shape;
    size_t i = 0;
    for (auto dim_size : var->get_shape())
    reduced_shape.push_back(std::ceil(dim_size/(stride[i++] * 1.0)));
    
    std::unordered_map<sg4::ActorPtr, std::vector<size_t>> local_reduced_start;
    std::unordered_map<sg4::ActorPtr, std::vector<size_t>> local_reduced_count;
    
    parametrization->set_reduced_shape(reduced_shape);
  }
};
///\endcond
} // namespace dtlmod
#endif //__DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__