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

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION

class ParameterizedDecimation {
  friend class DecimationReductionMethod;
  std::vector<size_t> stride_;
  std::string interpolation_method_ = "";
  double cost_per_element_;

  std::vector<size_t> reduced_shape_;
  std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>> reduced_local_start_and_count_;
  size_t element_size_;

protected:
  const std::vector<size_t>& get_stride() const { return stride_; }

  void set_reduced_shape(const std::vector<size_t>& reduced_shape) { reduced_shape_ = reduced_shape; }
  void set_reduced_local_start_and_count(
      std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>>
          reduced_local_start_and_count)
  {
    reduced_local_start_and_count_ = reduced_local_start_and_count;
  }

  size_t get_global_reduced_size() const
  {
    size_t total_size = element_size_;
    for (const auto& s : reduced_shape_)
      total_size *= s;
    return total_size;
  }

  size_t get_local_reduced_size() const
  {
    size_t total_size = element_size_;
    auto issuer       = sg4::Actor::self();
    for (const auto& c : reduced_local_start_and_count_.at(issuer).second)
      total_size *= c;
    return total_size;
  }

public:
  ParameterizedDecimation(const std::vector<size_t> stride, const std::string interpolation_method,
                         double cost_per_element, size_t element_size)
      : stride_(stride)
      , interpolation_method_(interpolation_method)
      , cost_per_element_(cost_per_element)
      , element_size_(element_size)
  {
  }
};

class DecimationReductionMethod : public ReductionMethod {
  std::map<std::shared_ptr<Variable>, std::shared_ptr<ParameterizedDecimation>> per_variable_parameterizations_;

protected:
  void parameterize_for_variable(std::shared_ptr<Variable> var,
                                const std::map<std::string, std::string>& parameters) override
  {
    std::vector<size_t> stride;
    std::string interpolation_method = "";
    double cost_per_element          = 1.0;

    for (const auto& [key, value] : parameters) {
      if (key == "stride") {
        std::vector<std::string> tokens;
        boost::split(tokens, value, boost::is_any_of(","), boost::token_compress_on);
        // TODO Add Sanity check that the number of tokens equals the number of dimension of var
        for (const auto t : tokens)
          stride.push_back(std::stoul(t));
      } else if (key == "interpolation") {
        interpolation_method = value;
      } else if (key == "cost_per_element") {
        cost_per_element = std::stod(value);
      } // else
        // TODO handle invalid key
    }

    per_variable_parameterizations_.try_emplace(
        var, std::make_shared<ParameterizedDecimation>(stride, interpolation_method, cost_per_element,
                                                      var->get_element_size()));
  }

  void reduce_variable(std::shared_ptr<Variable> var)
  {
    auto parameterization = per_variable_parameterizations_[var];
    auto shape           = var->get_shape();
    auto stride          = parameterization->get_stride();

    std::vector<size_t> reduced_shape;
    size_t i = 0;
    for (auto dim_size : shape)
      reduced_shape.push_back(std::ceil(dim_size / (stride[i++] * 1.0)));

    std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>>
        reduced_local_start_and_count;
    for (const auto& [actor, start_and_count] : var->get_local_start_and_count()) {
      auto [start, count] = start_and_count;
      std::vector<size_t> reduced_start;
      std::vector<size_t> reduced_count;

      for (size_t i = 0; i < shape.size(); i++) {
        // Sanity checks that shape, start, and count have the same size have already been done
        size_t r_start = std::ceil(start[i] / (stride[i] * 1.0));
        size_t r_next_start =
            std::min(shape[i], static_cast<size_t>(std::ceil((start[i] + count[i]) / (stride[i] * 1.0))));
        XBT_CDEBUG(dtlmod,"Dim %lu: stride = %lu, Start = %lu, r_start = %lu, Count = %lu, r_count = %lu",
                  i, stride[i], start[i], r_start, count[i], r_next_start - r_start);
        reduced_start.push_back(r_start);
        reduced_count.push_back(r_next_start - r_start);
      }
      reduced_local_start_and_count.try_emplace(actor, std::make_pair(reduced_start, reduced_count));
    }

    parameterization->set_reduced_shape(reduced_shape);
    parameterization->set_reduced_local_start_and_count(reduced_local_start_and_count);
  }

  size_t get_reduced_variable_global_size(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_global_reduced_size();
  }

  size_t get_reduced_variable_local_size(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_local_reduced_size();
  }

public:
  DecimationReductionMethod(const std::string& name) : ReductionMethod(name) {}
};
///\endcond
} // namespace dtlmod
#endif //__DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__