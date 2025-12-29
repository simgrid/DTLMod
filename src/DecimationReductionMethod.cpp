/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <cmath>
#include <numeric> // for std::accumulate

#include "dtlmod/DecimationReductionMethod.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_decimation_reduction, dtlmod, "DTL logging about decimation method");

namespace dtlmod {

size_t ParameterizedDecimation::get_global_reduced_size() const
{
  return std::accumulate(reduced_shape_.begin(), reduced_shape_.end(), var_->get_element_size(), std::multiplies<>{});
}

size_t ParameterizedDecimation::get_local_reduced_size() const
{
  auto start_and_count = reduced_local_start_and_count_.at(sg4::Actor::self()).second;
  return std::accumulate(start_and_count.begin(), start_and_count.end(), var_->get_element_size(), std::multiplies<>{});
}

const std::pair<std::vector<size_t>, std::vector<size_t>>&
ParameterizedDecimation::get_reduced_start_and_count_for(sg4::ActorPtr publisher) const
{
  return reduced_local_start_and_count_.at(publisher);
}

double ParameterizedDecimation::get_flop_amount_to_decimate() const
{
  XBT_DEBUG("Compute decimation cost with: cost_per_element = %.2f and interpolation_method = %s", cost_per_element_,
            interpolation_method_.c_str());
  double amount = cost_per_element_;
  if (interpolation_method_.empty()) {
    amount *= var_->get_local_size();
  } else if (interpolation_method_ == "linear") {
    amount = 2 * amount * var_->get_local_size();
  } else if (interpolation_method_ == "quadratic") {
    amount = 4 * amount * var_->get_local_size();
  } else if (interpolation_method_ == "cubic") {
    amount = 8 * amount * var_->get_local_size();
  } // Sanity check done when parameterizing the reduction method for this variable
  return amount;
}

void DecimationReductionMethod::parameterize_for_variable(std::shared_ptr<Variable> var,
                                                          const std::map<std::string, std::string>& parameters)
{
  std::vector<size_t> new_stride;
  std::string new_interpolation_method = "";
  double new_cost_per_element          = 1.0;

  // Detect existing parameterization (if any).
  auto it           = per_variable_parameterizations_.find(var);
  const bool exists = (it != per_variable_parameterizations_.end());

  // Initialize from existing values (if present) to support partial updates.
  if (exists) {
    const auto& existing = it->second;
    // Replace these getters with your actual API:
    new_stride               = existing->get_stride();
    new_interpolation_method = existing->get_interpolation_method();
    new_cost_per_element     = existing->get_cost_per_element();
  }

  for (const auto& [key, value] : parameters) {
    if (key == "stride") {
      std::vector<std::string> tokens;
      boost::split(tokens, value, boost::is_any_of(","), boost::token_compress_on);

      if (var->get_shape().size() != tokens.size())
        throw InconsistentDecimationStrideException(
            XBT_THROW_POINT, "Decimation Stride and Variable Shape vectors must have the same size. Stride: " +
                                 std::to_string(tokens.size()) + ", Shape: " + std::to_string(var->get_shape().size()));

      std::vector<size_t> parsed_stride;
      parsed_stride.reserve(tokens.size());
      for (const auto& t : tokens) {
        auto dim_stride = std::stoul(t);
        if (t[0] == '-' || dim_stride == 0)
          throw InconsistentDecimationStrideException(XBT_THROW_POINT, "Stride values must be strictly positive");
        parsed_stride.push_back(dim_stride);
      }
      new_stride = std::move(parsed_stride);

    } else if (key == "interpolation") {
      if (value == "linear" || value == "quadratic" || value == "cubic")
        new_interpolation_method = value;
      else
        throw UnknownDecimationInterpolationException(XBT_THROW_POINT,
                                                      std::string("Unknown interpolation method: ") + value +
                                                          " (options are: linear, cubic, or quadratic).");

      if ((value == "quadratic" && var->get_shape().size() < 2) || (value == "cubic" && var->get_shape().size() < 3))
        throw InconsistentDecimationInterpolationException(
            XBT_THROW_POINT, "Variable has not enough dimensions to apply this interpolation method");
    } else if (key == "cost_per_element")
      new_cost_per_element = std::stod(value);
    else
      throw UnknownDecimationOptionException(XBT_THROW_POINT, key.c_str());
  }

  if (!exists) {
    // First-time parameterization
    per_variable_parameterizations_.try_emplace(
        var,
        std::make_shared<ParameterizedDecimation>(var, new_stride, new_interpolation_method, new_cost_per_element));
    return;
  }

  // If already exists, update only if changed.
  const auto& existing = it->second;

  // Compare with existing to avoid unnecessary churn
  if (existing->get_stride() != new_stride)
    existing->set_stride(new_stride);
  if (existing->get_interpolation_method() != new_interpolation_method)
    existing->set_interpolation_method(new_interpolation_method);
  if (existing->get_cost_per_element() != new_cost_per_element)
    existing->set_cost_per_element(new_cost_per_element);
}

void DecimationReductionMethod::reduce_variable(std::shared_ptr<Variable> var)
{
  auto parameterization = per_variable_parameterizations_[var];
  auto original_shape   = var->get_shape();
  auto stride           = parameterization->get_stride();

  std::vector<size_t> reduced_shape;
  size_t i = 0;
  for (auto dim_size : original_shape)
    reduced_shape.push_back(std::ceil(dim_size / (stride[i++] * 1.0)));
  parameterization->set_reduced_shape(reduced_shape);

  auto self           = sg4::Actor::self();
  auto [start, count] = var->get_local_start_and_count().at(self);
  std::vector<size_t> reduced_start;
  std::vector<size_t> reduced_count;

  for (size_t i = 0; i < original_shape.size(); i++) {
    // Sanity checks that shape, start, and count have the same size have already been done
    size_t r_start = std::ceil(start[i] / (stride[i] * 1.0));
    size_t r_next_start =
        std::min(original_shape[i], static_cast<size_t>(std::ceil((start[i] + count[i]) / (stride[i] * 1.0))));
    XBT_DEBUG("Dim %lu: stride = %lu, Start = %lu, r_start = %lu, Count = %lu, r_count = %lu", i, stride[i], start[i],
              r_start, count[i], r_next_start - r_start);
    reduced_start.push_back(r_start);
    reduced_count.push_back(r_next_start - r_start);
  }

  parameterization->set_reduced_local_start_and_count(self, reduced_start, reduced_count);
}
} // namespace dtlmod
