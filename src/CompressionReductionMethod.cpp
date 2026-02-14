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
  if (cfg_.ratio_variability <= 0.0)
    return cfg_.compression_ratio;
  // Deterministic noise from hash of (variable_name, transaction_id)
  size_t seed = std::hash<std::string>{}(var_->get_name()) ^ (std::hash<unsigned int>{}(transaction_id) << 1);
  // Map to [1 - variability, 1 + variability]
  double noise = 1.0 + cfg_.ratio_variability * (2.0 * (seed % 10001) / 10000.0 - 1.0);
  return std::max(1.0, cfg_.compression_ratio * noise);
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

double CompressionReductionMethod::derive_compression_ratio(double accuracy, std::string_view profile,
                                                            double data_smoothness)
{
  if (profile == "sz") {
    // SZ-like prediction-based compressor: empirical fit from published benchmarks on scientific data.
    // Higher smoothness -> better prediction -> higher ratio.
    double alpha = 3.0;
    double beta  = 0.8;
    return std::max(1.0, alpha * std::pow(-std::log10(accuracy), beta) * (0.5 + data_smoothness));
  }
  if (profile == "zfp") {
    // ZFP-like transform-based compressor: rate = bits-per-value derived from accuracy.
    // 64 bits (double) / rate gives the compression ratio.
    double rate = std::max(1.0, -std::log2(accuracy) + 1.0);
    return std::max(1.0, 64.0 / rate);
  }
  // "fixed" profile: ratio must be user-specified (handled by the caller)
  return 1.0;
}

void CompressionReductionMethod::validate_compressor_profile(std::string_view profile)
{
  if (profile != "fixed" && profile != "sz" && profile != "zfp")
    throw UnknownCompressionOptionException(XBT_THROW_POINT, "Unknown compressor profile: " + std::string(profile) +
                                                                 " (options are: fixed, sz, or zfp).");
}

double CompressionReductionMethod::resolve_compression_ratio(double ratio, bool ratio_explicitly_set, bool is_new,
                                                             std::string_view profile, double accuracy,
                                                             double data_smoothness)
{
  if (ratio_explicitly_set) {
    if (ratio < 1.0)
      throw InconsistentCompressionRatioException(XBT_THROW_POINT, "Compression ratio must be >= 1.0");
    return ratio;
  }
  if (is_new) {
    if (profile == "fixed")
      throw InconsistentCompressionRatioException(
          XBT_THROW_POINT, "Compressor profile 'fixed' requires an explicit 'compression_ratio' parameter.");
    return derive_compression_ratio(accuracy, profile, data_smoothness);
  }
  return ratio; // Keep existing ratio for partial updates without explicit ratio
}

void CompressionReductionMethod::parameterize_for_variable(
    const Variable& var, const std::map<std::string, std::string, std::less<>>& parameters)
{
  // Start from existing values (if any) to support partial updates.
  auto it     = per_variable_parameterizations_.find(&var);
  bool is_new = (it == per_variable_parameterizations_.end());

  CompressionConfig cfg = is_new ? CompressionConfig{} : it->second->get_config();

  bool ratio_explicitly_set = false;

  for (const auto& [key, value] : parameters) {
    if (key == "accuracy")
      cfg.accuracy = std::stod(value);
    else if (key == "compression_cost_per_element")
      cfg.compression_cost_per_element = std::stod(value);
    else if (key == "decompression_cost_per_element")
      cfg.decompression_cost_per_element = std::stod(value);
    else if (key == "compression_ratio") {
      cfg.compression_ratio = std::stod(value);
      ratio_explicitly_set  = true;
    } else if (key == "compressor") {
      validate_compressor_profile(value);
      cfg.compressor_profile = value;
    } else if (key == "data_smoothness")
      cfg.data_smoothness = std::stod(value);
    else if (key == "ratio_variability")
      cfg.ratio_variability = std::stod(value);
    else
      throw UnknownCompressionOptionException(XBT_THROW_POINT, key);
  }

  cfg.compression_ratio = resolve_compression_ratio(cfg.compression_ratio, ratio_explicitly_set, is_new,
                                                    cfg.compressor_profile, cfg.accuracy, cfg.data_smoothness);

  XBT_DEBUG("Compression parameterization for Variable %s: profile=%s, accuracy=%.2e, ratio=%.2f, "
            "compression_cost=%.2f, decompression_cost=%.2f, smoothness=%.2f, variability=%.2f",
            var.get_cname(), cfg.compressor_profile.c_str(), cfg.accuracy, cfg.compression_ratio,
            cfg.compression_cost_per_element, cfg.decompression_cost_per_element, cfg.data_smoothness,
            cfg.ratio_variability);

  // Always (re)create the parameterization — avoids field-by-field update complexity.
  per_variable_parameterizations_[&var] = std::make_shared<ParameterizedCompression>(var, std::move(cfg));
}
} // namespace dtlmod
