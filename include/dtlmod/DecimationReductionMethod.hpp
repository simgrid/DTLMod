/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__
#define __DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__

#include "dtlmod/DTLException.hpp"
#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/Variable.hpp"

namespace dtlmod {

/// \cond EXCLUDE_FROM_DOCUMENTATION

class DecimationReductionMethod : public ReductionMethod {
  class ParameterizedDecimation {
    const Variable* var_; // non-owning: the Variable outlives the parameterization (both owned by Stream)

    std::vector<size_t> stride_;
    std::string interpolation_method_;
    double cost_per_element_;

    std::vector<size_t> reduced_shape_;
    std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>>
        reduced_local_start_and_count_;

  public:
    ParameterizedDecimation(const Variable& var, const std::vector<size_t>& stride,
                            const std::string& interpolation_method, double cost_per_element)
        : var_(&var), stride_(stride), interpolation_method_(interpolation_method), cost_per_element_(cost_per_element)
    {
    }

    void set_reduced_shape(const std::vector<size_t>& reduced_shape) { reduced_shape_ = reduced_shape; }
    void set_reduced_local_start_and_count(sg4::ActorPtr actor, const std::vector<size_t>& reduced_local_start,
                                           const std::vector<size_t>& reduced_local_count)
    {
      reduced_local_start_and_count_.try_emplace(actor, reduced_local_start, reduced_local_count);
    }

    [[nodiscard]] const std::vector<size_t>& get_stride() const { return stride_; }
    void set_stride(const std::vector<size_t>& stride) { stride_ = stride; }
    [[nodiscard]] const std::string& get_interpolation_method() const { return interpolation_method_; }
    void set_interpolation_method(std::string_view method) { interpolation_method_ = method; }
    [[nodiscard]] double get_cost_per_element() const { return cost_per_element_; }
    void set_cost_per_element(double cost) { cost_per_element_ = cost; }

    [[nodiscard]] const std::vector<size_t>& get_reduced_shape() const { return reduced_shape_; }

    [[nodiscard]] size_t get_global_reduced_size() const;
    [[nodiscard]] size_t get_local_reduced_size() const;
    [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
    get_reduced_start_and_count_for(sg4::ActorPtr publisher) const;
    [[nodiscard]] double get_flop_amount_to_decimate() const;
  };

  std::map<const Variable*, std::shared_ptr<ParameterizedDecimation>> per_variable_parameterizations_;

  /// @brief Parse and validate a comma-separated stride string against a variable's shape.
  static std::vector<size_t> parse_stride(std::string_view value, const Variable& var);

  /// @brief Validate that an interpolation method is compatible with the variable's dimensionality.
  static void validate_interpolation(std::string_view method, const Variable& var);

protected:
  void parameterize_for_variable(const Variable& var,
                                 const std::map<std::string, std::string, std::less<>>& parameters) override;

  void reduce_variable(const Variable& var) override;

  [[nodiscard]] size_t get_reduced_variable_global_size(const Variable& var) const override
  {
    return per_variable_parameterizations_.at(&var)->get_global_reduced_size();
  }

  [[nodiscard]] size_t get_reduced_variable_local_size(const Variable& var) const override
  {
    return per_variable_parameterizations_.at(&var)->get_local_reduced_size();
  }

  [[nodiscard]] double get_flop_amount_to_reduce_variable(const Variable& var) const override
  {
    return per_variable_parameterizations_.at(&var)->get_flop_amount_to_decimate();
  }

  [[nodiscard]] const std::vector<size_t>& get_reduced_variable_shape(const Variable& var) const override
  {
    return per_variable_parameterizations_.at(&var)->get_reduced_shape();
  }

  [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(const Variable& var, sg4::ActorPtr publisher) const override
  {
    return per_variable_parameterizations_.at(&var)->get_reduced_start_and_count_for(publisher);
  }

public:
  using ReductionMethod::ReductionMethod;
};
///\endcond
} // namespace dtlmod
#endif //__DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__
