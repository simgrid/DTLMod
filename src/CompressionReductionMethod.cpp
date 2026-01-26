/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <cmath>
#include <numeric> // for std::accumulate

#include "dtlmod/CompressionReductionMethod.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_compression_reduction, dtlmod, "DTL logging about compression method");

namespace dtlmod {

void CompressionReductionMethod::parameterize_for_variable(std::shared_ptr<Variable> var,
                                                           const std::map<std::string, std::string>& parameters)
{
  double new_accuracy                       = 1.0;
  double new_compression_cost_per_element   = 1.0;
  double new_decompression_cost_per_element = 1.0;

  // Detect existing parameterization (if any).
  auto it           = per_variable_parameterizations_.find(var);
  const bool exists = (it != per_variable_parameterizations_.end());

  // Initialize from existing values (if present) to support partial updates.
  if (exists) {
    const auto& existing               = it->second;
    new_accuracy                       = existing->get_accuracy();
    new_compression_cost_per_element   = existing->get_compression_cost_per_element();
    new_decompression_cost_per_element = existing->get_decompression_cost_per_element();
  }

  for (const auto& [key, value] : parameters) {
    if (key == "accuracy") {
      new_accuracy = std::stod(value);
    } else if (key == "compression_cost_per_element") {
      new_compression_cost_per_element = std::stod(value);
    } else if (key == "decompression_cost_per_element") {
      new_decompression_cost_per_element = std::stod(value);
    } else {
      throw UnknownCompressionOptionException(XBT_THROW_POINT, key.c_str());
    }
  }

  if (!exists) {
    // First-time parameterization
    per_variable_parameterizations_.try_emplace(
        var, std::make_shared<ParameterizedCompression>(new_accuracy, new_compression_cost_per_element,
                                                        new_decompression_cost_per_element));
    return;
  }

  // If already exists, update only if changed.
  const auto& existing = it->second;

  // Compare with existing to avoid unnecessary churn
  if (existing->get_accuracy() != new_accuracy)
    existing->set_accuracy(new_accuracy);
  if (existing->get_compression_cost_per_element() != new_compression_cost_per_element)
    existing->set_compression_cost_per_element(new_compression_cost_per_element);
  if (existing->get_decompression_cost_per_element() != new_decompression_cost_per_element)
    existing->set_decompression_cost_per_element(new_decompression_cost_per_element);
}
} // namespace dtlmod
