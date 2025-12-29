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

class ParameterizedDecimation {
  friend class DecimationReductionMethod;
  std::shared_ptr<Variable> var_; // The variable to which this parameterized decimation is applied

  std::vector<size_t> stride_;
  std::string interpolation_method_ = "";
  double cost_per_element_;

  std::vector<size_t> reduced_shape_;
  std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>> reduced_local_start_and_count_;

protected:
  void set_reduced_shape(const std::vector<size_t>& reduced_shape) { reduced_shape_ = reduced_shape; }
  void set_reduced_local_start_and_count(
      std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>>
          reduced_local_start_and_count)
  {
    reduced_local_start_and_count_ = reduced_local_start_and_count;
  }

  [[nodiscard]] const std::vector<size_t>& get_stride() const { return stride_; }
  void set_stride(const std::vector<size_t>& stride) { stride_ = stride; }
  [[nodiscard]] const std::string& get_interpolation_method() const { return interpolation_method_; }
  void set_interpolation_method(const std::string& method) { interpolation_method_ = method; }
  [[nodiscard]] double get_cost_per_element() const { return cost_per_element_; }
  void set_cost_per_element(double cost) { cost_per_element_ = cost; }

  [[nodiscard]] const std::vector<size_t>& get_reduced_shape() const { return reduced_shape_; }

  [[nodiscard]] size_t get_global_reduced_size() const;
  [[nodiscard]] size_t get_local_reduced_size() const;
  [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(sg4::ActorPtr publisher) const;
  [[nodiscard]] double get_flop_amount_to_decimate() const;

public:
  ParameterizedDecimation(std::shared_ptr<Variable> var, const std::vector<size_t> stride,
                          const std::string interpolation_method, double cost_per_element)
      : var_(var), stride_(stride), interpolation_method_(interpolation_method), cost_per_element_(cost_per_element)
  {
  }
};

class DecimationReductionMethod : public ReductionMethod {
  std::map<std::shared_ptr<Variable>, std::shared_ptr<ParameterizedDecimation>> per_variable_parameterizations_;

protected:
  void parameterize_for_variable(std::shared_ptr<Variable> var,
                                 const std::map<std::string, std::string>& parameters) override;

  void reduce_variable(std::shared_ptr<Variable> var);

  [[nodiscard]] size_t get_reduced_variable_global_size(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_global_reduced_size();
  }

  [[nodiscard]] size_t get_reduced_variable_local_size(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_local_reduced_size();
  }

  [[nodiscard]] double get_flop_amount_to_reduce_variable(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_flop_amount_to_decimate();
  }

  [[nodiscard]] const std::vector<size_t>& get_reduced_variable_shape(std::shared_ptr<Variable> var) const override
  {
    return per_variable_parameterizations_.at(var)->get_reduced_shape();
  }

  [[nodiscard]] const std::pair<std::vector<size_t>, std::vector<size_t>>&
  get_reduced_start_and_count_for(std::shared_ptr<Variable> var, sg4::ActorPtr publisher) const override
  {
    return per_variable_parameterizations_.at(var)->get_reduced_start_and_count_for(publisher);
  }

public:
  DecimationReductionMethod(const std::string& name) : ReductionMethod(name) {}
};
///\endcond
} // namespace dtlmod
#endif //__DTLMOD_DECIMATION_REDUCTION_METHOD_HPP__