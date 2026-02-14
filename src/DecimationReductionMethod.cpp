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

size_t DecimationReductionMethod::ParameterizedDecimation::get_global_reduced_size() const
{
  return std::accumulate(reduced_shape_.begin(), reduced_shape_.end(), var_->get_element_size(), std::multiplies<>{});
}

size_t DecimationReductionMethod::ParameterizedDecimation::get_local_reduced_size() const
{
  auto start_and_count = reduced_local_start_and_count_.at(sg4::Actor::self()).second;
  return std::accumulate(start_and_count.begin(), start_and_count.end(), var_->get_element_size(), std::multiplies<>{});
}

const std::pair<std::vector<size_t>, std::vector<size_t>>&
DecimationReductionMethod::ParameterizedDecimation::get_reduced_start_and_count_for(sg4::ActorPtr publisher) const
{
  return reduced_local_start_and_count_.at(publisher);
}

double DecimationReductionMethod::ParameterizedDecimation::get_flop_amount_to_decimate() const
{
  XBT_DEBUG("Compute decimation cost with: cost_per_element = %.2f and interpolation_method = %s", cost_per_element_,
            interpolation_method_.c_str());
  double amount   = cost_per_element_;
  auto local_size = static_cast<double>(var_->get_local_size());
  int multiplier  = 1;

  if (interpolation_method_ == "linear")
    multiplier = 2;
  else if (interpolation_method_ == "quadratic")
    multiplier = 4;
  else if (interpolation_method_ == "cubic")
    multiplier = 8;

  return multiplier * amount * local_size;
}

std::vector<size_t> DecimationReductionMethod::parse_stride(std::string_view value, const Variable& var)
{
  std::vector<std::string> tokens;
  std::string value_str(value);
  boost::split(tokens, value_str, boost::is_any_of(","), boost::token_compress_on);

  if (var.get_shape().size() != tokens.size())
    throw InconsistentDecimationStrideException(
        XBT_THROW_POINT, "Decimation Stride and Variable Shape vectors must have the same size. Stride: " +
                             std::to_string(tokens.size()) + ", Shape: " + std::to_string(var.get_shape().size()));

  std::vector<size_t> stride;
  stride.reserve(tokens.size());
  for (const auto& t : tokens) {
    auto dim_stride = std::stoul(t);
    if (t[0] == '-' || dim_stride == 0)
      throw InconsistentDecimationStrideException(XBT_THROW_POINT, "Stride values must be strictly positive");
    stride.push_back(dim_stride);
  }
  return stride;
}

void DecimationReductionMethod::validate_interpolation(std::string_view method, const Variable& var)
{
  if (method != "linear" && method != "quadratic" && method != "cubic")
    throw UnknownDecimationInterpolationException(XBT_THROW_POINT, std::string("Unknown interpolation method: ") +
                                                                       std::string(method) +
                                                                       " (options are: linear, cubic, or quadratic).");

  if ((method == "quadratic" && var.get_shape().size() < 2) || (method == "cubic" && var.get_shape().size() < 3))
    throw InconsistentDecimationInterpolationException(
        XBT_THROW_POINT, "Variable has not enough dimensions to apply this interpolation method");
}

void DecimationReductionMethod::parameterize_for_variable(
    const Variable& var, const std::map<std::string, std::string, std::less<>>& parameters)
{
  // Start from existing values (if any) to support partial updates.
  auto it = per_variable_parameterizations_.find(&var);

  std::vector<size_t> stride;
  std::string interpolation_method;
  double cost_per_element = 1.0;

  if (it != per_variable_parameterizations_.end()) {
    stride               = it->second->get_stride();
    interpolation_method = it->second->get_interpolation_method();
    cost_per_element     = it->second->get_cost_per_element();
  }

  for (const auto& [key, value] : parameters) {
    if (key == "stride")
      stride = parse_stride(value, var);
    else if (key == "interpolation") {
      validate_interpolation(value, var);
      interpolation_method = value;
    } else if (key == "cost_per_element")
      cost_per_element = std::stod(value);
    else
      throw UnknownDecimationOptionException(XBT_THROW_POINT, key);
  }

  // Always (re)create the parameterization — avoids field-by-field update complexity.
  per_variable_parameterizations_[&var] =
      std::make_shared<ParameterizedDecimation>(var, stride, interpolation_method, cost_per_element);
}

void DecimationReductionMethod::reduce_variable(const Variable& var)
{
  auto parameterization = per_variable_parameterizations_[&var];
  auto original_shape   = var.get_shape();
  auto stride           = parameterization->get_stride();

  std::vector<size_t> reduced_shape;
  size_t idx = 0;
  for (auto dim_size : original_shape) {
    reduced_shape.push_back(
        static_cast<size_t>(std::ceil(static_cast<double>(dim_size) / static_cast<double>(stride[idx]))));
    idx++;
  }
  parameterization->set_reduced_shape(reduced_shape);

  auto self           = sg4::Actor::self();
  auto [start, count] = var.get_local_start_and_count(self);
  std::vector<size_t> reduced_start;
  std::vector<size_t> reduced_count;

  for (size_t i = 0; i < original_shape.size(); i++) {
    // Sanity checks that shape, start, and count have the same size have already been done
    auto r_start = static_cast<size_t>(std::ceil(static_cast<double>(start[i]) / static_cast<double>(stride[i])));
    size_t r_next_start = std::min(
        original_shape[i],
        static_cast<size_t>(std::ceil(static_cast<double>(start[i] + count[i]) / static_cast<double>(stride[i]))));
    XBT_DEBUG("Dim %zu: stride = %zu, Start = %zu, r_start = %zu, Count = %zu, r_count = %zu", i, stride[i], start[i],
              r_start, count[i], r_next_start - r_start);
    reduced_start.push_back(r_start);
    reduced_count.push_back(r_next_start - r_start);
  }

  parameterization->set_reduced_local_start_and_count(self, reduced_start, reduced_count);
}
} // namespace dtlmod
