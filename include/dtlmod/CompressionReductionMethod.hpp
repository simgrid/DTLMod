/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__
#define __DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__

#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/Variable.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION

class CompressionReductionMethod : public ReductionMethod {
  struct CompressionConfig {
    double accuracy                       = 1e-3;
    double compression_cost_per_element   = 1.0;
    double decompression_cost_per_element = 1.0;
    double compression_ratio              = 0.0;
    std::string compressor_profile        = "fixed";
    double data_smoothness                = 0.5;
    double ratio_variability              = 0.0;
  };

  class ParameterizedCompression {
    const Variable* var_; // non-owning: the Variable outlives the parameterization (both owned by Stream)
    CompressionConfig cfg_;

  public:
    ParameterizedCompression(const Variable& var, CompressionConfig cfg) : var_(&var), cfg_(std::move(cfg)) {}

    [[nodiscard]] double get_accuracy() const { return cfg_.accuracy; }
    [[nodiscard]] double get_compression_cost_per_element() const { return cfg_.compression_cost_per_element; }
    [[nodiscard]] double get_decompression_cost_per_element() const { return cfg_.decompression_cost_per_element; }
    [[nodiscard]] double get_compression_ratio() const { return cfg_.compression_ratio; }
    [[nodiscard]] const std::string& get_compressor_profile() const { return cfg_.compressor_profile; }
    [[nodiscard]] double get_data_smoothness() const { return cfg_.data_smoothness; }
    [[nodiscard]] double get_ratio_variability() const { return cfg_.ratio_variability; }
    [[nodiscard]] const CompressionConfig& get_config() const { return cfg_; }

    /// @brief Get the effective compression ratio, optionally perturbed by per-transaction noise.
    [[nodiscard]] double get_effective_ratio(unsigned int transaction_id = 0) const;
  };

  std::map<const Variable*, std::shared_ptr<ParameterizedCompression>> per_variable_parameterizations_;

  /// @brief Derive the compression ratio from accuracy and compressor profile.
  static double derive_compression_ratio(double accuracy, std::string_view profile, double data_smoothness);

  /// @brief Validate compressor profile string. Throws if invalid.
  static void validate_compressor_profile(std::string_view profile);

  /// @brief Validate and resolve the compression ratio from parsed parameters.
  static double resolve_compression_ratio(double ratio, bool ratio_explicitly_set, bool is_new,
                                          std::string_view profile, double accuracy, double data_smoothness);

public:
  using ReductionMethod::ReductionMethod;
  void parameterize_for_variable(const Variable& var,
                                 const std::map<std::string, std::string, std::less<>>& parameters) override;
  void reduce_variable(const Variable& /* var*/) override
  { /* Variable metadata are not modfied when using compression */ }

  [[nodiscard]] size_t get_reduced_variable_global_size(const Variable& var) const override;
  [[nodiscard]] size_t get_reduced_variable_local_size(const Variable& var) const override;

  [[nodiscard]] const std::vector<size_t>& get_reduced_variable_shape(const Variable& var) const override
  {
    return var.get_shape();
  }

  [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(const Variable& var, sg4::ActorPtr publisher) const override
  {
    return var.get_local_start_and_count(publisher);
  }

  [[nodiscard]] double get_flop_amount_to_reduce_variable(const Variable& var) const override;
  [[nodiscard]] double get_flop_amount_to_decompress_variable(const Variable& var) const override;
};
/// \endcond
} // namespace dtlmod
#endif //__DTLMOD_COMPRESSION_REDUCTION_METHOD_HPP__
