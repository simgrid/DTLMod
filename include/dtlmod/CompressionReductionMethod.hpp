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
  class ParameterizedCompression {
    const Variable* var_; // non-owning: the Variable outlives the parameterization (both owned by Stream)
    double accuracy_;
    double compression_cost_per_element_;
    double decompression_cost_per_element_;
    double compression_ratio_;
    std::string compressor_profile_; // "fixed", "sz", "zfp"
    double data_smoothness_;         // hint in [0,1], shifts the model curve
    double ratio_variability_;       // per-transaction noise amplitude in [0,1]

  public:
    ParameterizedCompression(const Variable& var, double accuracy, double compression_cost_per_element,
                             double decompression_cost_per_element, double compression_ratio,
                             const std::string& compressor_profile, double data_smoothness, double ratio_variability)
        : var_(&var)
        , accuracy_(accuracy)
        , compression_cost_per_element_(compression_cost_per_element)
        , decompression_cost_per_element_(decompression_cost_per_element)
        , compression_ratio_(compression_ratio)
        , compressor_profile_(compressor_profile)
        , data_smoothness_(data_smoothness)
        , ratio_variability_(ratio_variability)
    {
    }

    [[nodiscard]] double get_accuracy() const { return accuracy_; }
    void set_accuracy(double accuracy) { accuracy_ = accuracy; }
    [[nodiscard]] double get_compression_cost_per_element() const { return compression_cost_per_element_; }
    void set_compression_cost_per_element(double cost) { compression_cost_per_element_ = cost; }
    [[nodiscard]] double get_decompression_cost_per_element() const { return decompression_cost_per_element_; }
    void set_decompression_cost_per_element(double cost) { decompression_cost_per_element_ = cost; }
    [[nodiscard]] double get_compression_ratio() const { return compression_ratio_; }
    void set_compression_ratio(double ratio) { compression_ratio_ = ratio; }
    [[nodiscard]] const std::string& get_compressor_profile() const { return compressor_profile_; }
    void set_compressor_profile(const std::string& profile) { compressor_profile_ = profile; }
    [[nodiscard]] double get_data_smoothness() const { return data_smoothness_; }
    void set_data_smoothness(double smoothness) { data_smoothness_ = smoothness; }
    [[nodiscard]] double get_ratio_variability() const { return ratio_variability_; }
    void set_ratio_variability(double variability) { ratio_variability_ = variability; }

    /// @brief Get the effective compression ratio, optionally perturbed by per-transaction noise.
    [[nodiscard]] double get_effective_ratio(unsigned int transaction_id = 0) const;
  };

  std::map<const Variable*, std::shared_ptr<ParameterizedCompression>> per_variable_parameterizations_;

  /// @brief Derive the compression ratio from accuracy and compressor profile.
  static double derive_compression_ratio(double accuracy, const std::string& profile, double data_smoothness);

public:
  CompressionReductionMethod(const std::string& name) : ReductionMethod(name) {}
  void parameterize_for_variable(const Variable& var, const std::map<std::string, std::string>& parameters) override;
  void reduce_variable(const Variable& /* var*/) override {}

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
