/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_VARIABLE_HPP__
#define __DTLMOD_VARIABLE_HPP__

#include <string>
#include <unordered_map>
#include <vector>

#include "dtlmod/Metadata.hpp"
#include "dtlmod/ReductionMethod.hpp"

namespace dtlmod {

class Stream;

/// @brief A class to translate a piece of data from an application into an object handled by the DTL and its metadata.
class Variable : public std::enable_shared_from_this<Variable> {
  friend class Engine;
  friend class Stream;

  enum class ReductionOrigin { None, Publisher, Subscriber };

  std::string name_;
  size_t element_size_;
  std::vector<size_t> shape_;
  std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>> local_start_and_count_;
  unsigned int transaction_start_ = 0;
  unsigned int transaction_count_ = 0;

  std::weak_ptr<const Stream> defined_in_stream_;

  std::shared_ptr<Metadata> metadata_;

  std::map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>, std::less<>> subscriber_selections_;
  std::map<sg4::ActorPtr, std::pair<unsigned int, unsigned int>, std::less<>> subscriber_transaction_selections_;
  std::shared_ptr<ReductionMethod> is_reduced_with_ = nullptr;
  ReductionOrigin reduction_origin_{ReductionOrigin::None};

protected:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  void create_metadata() { metadata_ = std::make_shared<Metadata>(shared_from_this()); }
  void set_metadata(std::shared_ptr<Metadata> metadata) { metadata_ = metadata; }
  /// \endcond

public:
  /// \cond EXCLUDE_FROM_DOCUMENTATION
  // Public accessors for Transport classes
  void set_transaction_start(unsigned int start) noexcept { transaction_start_ = start; }
  [[nodiscard]] unsigned int get_transaction_start() const noexcept { return transaction_start_; }
  void set_transaction_count(unsigned int count) noexcept { transaction_count_ = count; }
  [[nodiscard]] unsigned int get_transaction_count() const noexcept { return transaction_count_; }

  void set_local_start_and_count(sg4::ActorPtr actor,
                                 const std::pair<std::vector<size_t>, std::vector<size_t>>& local_start_and_count)
  {
    local_start_and_count_[actor] = local_start_and_count;
  }
  const std::pair<std::vector<size_t>, std::vector<size_t>>& get_local_start_and_count(sg4::ActorPtr actor) const
  {
    return local_start_and_count_.at(actor);
  }

  void add_transaction_metadata(unsigned int transaction_id, sg4::ActorPtr publisher, const std::string& location);
  std::vector<std::pair<std::string, sg_size_t>> get_sizes_to_get_per_block(unsigned int transaction_id,
                                                                            const std::vector<size_t>& start,
                                                                            const std::vector<size_t>& count) const;

  std::shared_ptr<Metadata> get_metadata() const { return metadata_; }
  [[nodiscard]] bool subscriber_has_a_selection(sg4::ActorPtr actor) const;
  [[nodiscard]] bool subscriber_has_a_transaction_selection(sg4::ActorPtr actor) const;
  const std::pair<std::vector<size_t>, std::vector<size_t>>& get_subscriber_selection(sg4::ActorPtr actor) const;
  const std::pair<unsigned int, unsigned int>& get_subscriber_transaction_selection(sg4::ActorPtr actor) const;
  /// \endcond

  /// \cond EXCLUDE_FROM_DOCUMENTATION
  Variable(const std::string& name, size_t element_size, const std::vector<size_t>& shape,
           std::shared_ptr<const Stream> stream)
      : name_(name), element_size_(element_size), shape_(shape), defined_in_stream_(stream)

  {
  }

  Variable(const Variable&)            = delete;
  Variable& operator=(const Variable&) = delete;
  Variable(Variable&&)                 = delete;
  Variable& operator=(Variable&&)      = delete;
  /// \endcond

  /// @brief Helper function to print out the name of the Variable.
  /// @return The corresponding string.
  [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
  /// @brief Helper function to print out the name of the Variable.
  /// @return The corresponding C-string.
  [[nodiscard]] const char* get_cname() const noexcept { return name_.c_str(); }

  /// @brief Get the shape of the Variable.
  /// @return A vector of the respective size in each dimension of the Variable.
  [[nodiscard]] const std::vector<size_t>& get_shape() const noexcept { return shape_; }
  /// @brief Get the size of the elements stored in the Variable.
  /// @return The elements' size.
  [[nodiscard]] size_t get_element_size() const noexcept { return element_size_; }

  /// @brief Get the global size of the Variable.
  /// @return The computed size.
  [[nodiscard]] size_t get_global_size() const;
  /// @brief Get the local size object
  /// @return The computed size.
  [[nodiscard]] size_t get_local_size() const;

  /// @brief Allow a subscriber to select what subset of a variable it would like to get.
  /// @param start a vector of starting positions in each dimension of the Variable.
  /// @param count a vector of number of elements to get in each dimension.
  void set_selection(const std::vector<size_t>& start, const std::vector<size_t>& count);
  /// @brief Allow a subscriber to select what transaction it would like to get.
  /// @param transaction_id the id of the transaction to get.
  void set_transaction_selection(unsigned int transaction_id) { set_transaction_selection(transaction_id, 1); }
  /// @brief Allow a subscriber to select what transactions it would like to get.
  /// @param begin the id at which the range of transactions to get begins.
  /// @param count the number of transactions in the range.
  void set_transaction_selection(unsigned int begin, unsigned int count);

  /// @brief Assign a parameterized reduction method to the Variable.
  /// @param method a ReductionMethod (already defined).
  /// @param paramaters specific parameters in key-value form to apply the reduction method to the Variable.
  void set_reduction_operation(std::shared_ptr<ReductionMethod> method, std::map<std::string, std::string> parameters);

  [[nodiscard]] bool is_reduced() const { return is_reduced_with_ != nullptr; }

  [[nodiscard]] bool is_reduced_by_publisher() const noexcept
  {
    return reduction_origin_ == ReductionOrigin::Publisher;
  }
  [[nodiscard]] bool is_reduced_by_subscriber() const noexcept
  {
    return reduction_origin_ == ReductionOrigin::Subscriber;
  }

  [[nodiscard]] const std::shared_ptr<ReductionMethod>& get_reduction_method() const { return is_reduced_with_; }
};
} // namespace dtlmod

#endif
