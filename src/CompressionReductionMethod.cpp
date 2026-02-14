/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <cmath>
#include <functional>

#include "dtlmod/CompressionReductionMethod.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_compression_reduction, dtlmod, "DTL logging about compression method");

namespace dtlmod {

double CompressionReductionMethod::ParameterizedCompression::get_effective_ratio(unsigned int transaction_id) const
{
  if (ratio_variability_ <= 0.0)
    return compression_ratio_;
  // Deterministic noise from hash of (variable_name, transaction_id)
  size_t seed = std::hash<std::string>{}(var_->get_name()) ^ (std::hash<unsigned int>{}(transaction_id) << 1);
  // Map to [1 - variability, 1 + variability]
  double noise = 1.0 + ratio_variability_ * (2.0 * (seed % 10001) / 10000.0 - 1.0);
  return std::max(1.0, compression_ratio_ * noise);
}

size_t CompressionReductionMethod::get_reduced_variable_global_size(const Variable& var) const
{
  auto ratio = per_variable_parameterizations_.at(&var)->get_compression_ratio();
  return static_cast<size_t>(std::ceil(static_cast<double>(var.get_global_size()) / ratio));
}

size_t CompressionReductionMethod::get_reduced_variable_local_size(const Variable& var) const
{
  auto ratio = per_variable_parameterizations_.at(&var)->get_compression_ratio();
  return static_cast<size_t>(std::ceil(static_cast<double>(var.get_local_size()) / ratio));
}

double CompressionReductionMethod::get_flop_amount_to_reduce_variable(const Variable& var) const
{
  auto param        = per_variable_parameterizations_.at(&var);
  auto num_elements = static_cast<double>(var.get_local_size() / var.get_element_size());
  return param->get_compression_cost_per_element() * num_elements;
}

double CompressionReductionMethod::get_flop_amount_to_decompress_variable(const Variable& var) const
{
  auto param        = per_variable_parameterizations_.at(&var);
  auto num_elements = static_cast<double>(var.get_local_size() / var.get_element_size());
  return param->get_decompression_cost_per_element() * num_elements;
}

double CompressionReductionMethod::derive_compression_ratio(double accuracy, const std::string& profile,
                                                            double data_smoothness)
{
  if (profile == "sz") {
    // SZ-like prediction-based compressor: empirical fit from published benchmarks on scientific data.
    // Higher smoothness → better prediction → higher ratio.
    double alpha = 3.0;
    double beta  = 0.8;
    return std::max(1.0, alpha * std::pow(-std::log10(accuracy), beta) * (0.5 + data_smoothness));
  } else if (profile == "zfp") {
    // ZFP-like transform-based compressor: rate = bits-per-value derived from accuracy.
    // 64 bits (double) / rate gives the compression ratio.
    double rate = std::max(1.0, -std::log2(accuracy) + 1.0);
    return std::max(1.0, 64.0 / rate);
  }
  // "fixed" profile: ratio must be user-specified (handled by the caller)
  return 1.0;
}

void CompressionReductionMethod::parameterize_for_variable(
    const Variable& var, const std::map<std::string, std::string, std::less<>>& parameters)
{
  double new_accuracy                       = 1e-3;
  double new_compression_cost_per_element   = 1.0;
  double new_decompression_cost_per_element = 1.0;
  double new_compression_ratio              = 0.0; // 0 means "not specified, must be derived"
  std::string new_compressor_profile        = "fixed";
  double new_data_smoothness                = 0.5;
  double new_ratio_variability              = 0.0;

  // Detect existing parameterization (if any).
  auto it           = per_variable_parameterizations_.find(&var);
  const bool exists = (it != per_variable_parameterizations_.end());

  // Initialize from existing values (if present) to support partial updates.
  if (exists) {
    const auto& existing               = it->second;
    new_accuracy                       = existing->get_accuracy();
    new_compression_cost_per_element   = existing->get_compression_cost_per_element();
    new_decompression_cost_per_element = existing->get_decompression_cost_per_element();
    new_compression_ratio              = existing->get_compression_ratio();
    new_compressor_profile             = existing->get_compressor_profile();
    new_data_smoothness                = existing->get_data_smoothness();
    new_ratio_variability              = existing->get_ratio_variability();
  }

  bool ratio_explicitly_set = false;

  for (const auto& [key, value] : parameters) {
    if (key == "accuracy") {
      new_accuracy = std::stod(value);
    } else if (key == "compression_cost_per_element") {
      new_compression_cost_per_element = std::stod(value);
    } else if (key == "decompression_cost_per_element") {
      new_decompression_cost_per_element = std::stod(value);
    } else if (key == "compression_ratio") {
      new_compression_ratio = std::stod(value);
      ratio_explicitly_set  = true;
    } else if (key == "compressor") {
      if (value == "fixed" || value == "sz" || value == "zfp")
        new_compressor_profile = value;
      else
        throw UnknownCompressionOptionException(XBT_THROW_POINT, "Unknown compressor profile: " + value +
                                                                     " (options are: fixed, sz, or zfp).");
    } else if (key == "data_smoothness") {
      new_data_smoothness = std::stod(value);
    } else if (key == "ratio_variability") {
      new_ratio_variability = std::stod(value);
    } else {
      throw UnknownCompressionOptionException(XBT_THROW_POINT, key);
    }
  }

  // Derive compression ratio if not explicitly specified
  if (!ratio_explicitly_set && !exists) {
    if (new_compressor_profile == "fixed")
      throw InconsistentCompressionRatioException(
          XBT_THROW_POINT, "Compressor profile 'fixed' requires an explicit 'compression_ratio' parameter.");
    new_compression_ratio = derive_compression_ratio(new_accuracy, new_compressor_profile, new_data_smoothness);
  } else if (ratio_explicitly_set && new_compression_ratio < 1.0) {
    throw InconsistentCompressionRatioException(XBT_THROW_POINT, "Compression ratio must be >= 1.0");
  }

  XBT_DEBUG("Compression parameterization for Variable %s: profile=%s, accuracy=%.2e, ratio=%.2f, "
            "compression_cost=%.2f, decompression_cost=%.2f, smoothness=%.2f, variability=%.2f",
            var.get_cname(), new_compressor_profile.c_str(), new_accuracy, new_compression_ratio,
            new_compression_cost_per_element, new_decompression_cost_per_element, new_data_smoothness,
            new_ratio_variability);

  if (!exists) {
    per_variable_parameterizations_.try_emplace(
        &var, std::make_shared<ParameterizedCompression>(
                  var, new_accuracy, new_compression_cost_per_element, new_decompression_cost_per_element,
                  new_compression_ratio, new_compressor_profile, new_data_smoothness, new_ratio_variability));
    return;
  }

  // If already exists, update only if changed.
  const auto& existing = it->second;

  if (existing->get_accuracy() != new_accuracy)
    existing->set_accuracy(new_accuracy);
  if (existing->get_compression_cost_per_element() != new_compression_cost_per_element)
    existing->set_compression_cost_per_element(new_compression_cost_per_element);
  if (existing->get_decompression_cost_per_element() != new_decompression_cost_per_element)
    existing->set_decompression_cost_per_element(new_decompression_cost_per_element);
  if (ratio_explicitly_set || new_compressor_profile != existing->get_compressor_profile()) {
    double updated_ratio = ratio_explicitly_set
                               ? new_compression_ratio
                               : derive_compression_ratio(new_accuracy, new_compressor_profile, new_data_smoothness);
    existing->set_compression_ratio(updated_ratio);
  }
  if (existing->get_compressor_profile() != new_compressor_profile)
    existing->set_compressor_profile(new_compressor_profile);
  if (existing->get_data_smoothness() != new_data_smoothness)
    existing->set_data_smoothness(new_data_smoothness);
  if (existing->get_ratio_variability() != new_ratio_variability)
    existing->set_ratio_variability(new_ratio_variability);
}
} // namespace dtlmod
