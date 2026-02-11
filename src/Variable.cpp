/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/Variable.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/Stream.hpp"
#include <limits>
#include <numeric>
#include <stdexcept>

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_variable, dtlmod, "DTL logging about Variables");

namespace dtlmod {

struct checked_multiply {
  size_t operator()(size_t a, size_t b) const
  {
    if (b != 0 && a > std::numeric_limits<size_t>::max() / b) {
      throw std::overflow_error("size_t overflow in multiplication");
    }
    return a * b;
  }
};

////////////////////////////////////////////
///////////// PUBLIC INTERFACE /////////////
////////////////////////////////////////////

/// The global size of a Variable corresponds to the product of the number of elements in each dimension of the shape
/// vector by the element size.
size_t Variable::get_global_size() const
{
  return std::accumulate(shape_.begin(), shape_.end(), element_size_, dtlmod::checked_multiply{});
}

/// The local size of a Variable corresponds to the product of the number of elements in each dimension of the count
/// vector by the element size. If variable was published to the DTL over multiple transactions, multiply the size by
/// the number of transactions.
size_t Variable::get_local_size() const
{
  const auto& start_and_count = local_start_and_count_.at(sg4::Actor::self()).second;
  auto total_size =
      std::accumulate(start_and_count.begin(), start_and_count.end(), element_size_, dtlmod::checked_multiply{});
  if (transaction_count_ > 0)
    total_size *= transaction_count_;
  return total_size;
}

void Variable::set_selection(const std::vector<size_t>& start, const std::vector<size_t>& count)
{
  subscriber_selections_[sg4::Actor::self()] = std::make_pair(start, count);
}

void Variable::set_transaction_selection(unsigned int begin, unsigned int count)
{
  subscriber_transaction_selections_[sg4::Actor::self()] = std::make_pair(begin, count);
}

void Variable::set_reduction_operation(std::shared_ptr<ReductionMethod> method,
                                       std::map<std::string, std::string> parameters)
{
  if (is_reduced_with_ && reduction_origin_ == ReductionOrigin::Publisher &&
      defined_in_stream_.lock()->get_access_mode() == Stream::Mode::Subscribe) {
    XBT_ERROR("Subscriber %s attempted to re-reduce Variable %s, but it was already reduced on publisher side.",
              sg4::Actor::self()->get_cname(), this->get_cname());
    throw DoubleReductionException(
        XBT_THROW_POINT,
        "Variable has already been reduced by its producer; subscriber-side reduction is not allowed.");
  }

  method->parameterize_for_variable(shared_from_this(), parameters);
  method->reduce_variable(shared_from_this());
  is_reduced_with_ = method;
  if (defined_in_stream_.lock()->get_access_mode() == Stream::Mode::Publish)
    reduction_origin_ = ReductionOrigin::Publisher;
  else
    reduction_origin_ = ReductionOrigin::Subscriber;
}

////////////////////////////////////////////
///////////////// INTERNALS ////////////////
////////////////////////////////////////////

/// \cond EXCLUDE_FROM_DOCUMENTATION
bool Variable::subscriber_has_a_selection(sg4::ActorPtr actor) const
{
  return subscriber_selections_.find(actor) != subscriber_selections_.end();
}

bool Variable::subscriber_has_a_transaction_selection(sg4::ActorPtr actor) const
{
  return subscriber_transaction_selections_.find(actor) != subscriber_transaction_selections_.end();
}

const std::pair<std::vector<size_t>, std::vector<size_t>>& Variable::get_subscriber_selection(sg4::ActorPtr actor) const
{
  return subscriber_selections_.at(actor);
}

const std::pair<unsigned int, unsigned int>& Variable::get_subscriber_transaction_selection(sg4::ActorPtr actor) const
{
  return subscriber_transaction_selections_.at(actor);
}

void Variable::add_transaction_metadata(unsigned int transaction_id, sg4::ActorPtr publisher,
                                        const std::string& location)
{
  if (is_reduced_with_) {
    auto start_and_count = is_reduced_with_->get_reduced_start_and_count_for(shared_from_this(), publisher);
    metadata_->add_transaction(transaction_id, start_and_count, location, publisher);
  } else
    metadata_->add_transaction(transaction_id, local_start_and_count_[publisher], location, publisher);
}

std::vector<std::pair<std::string, sg_size_t>>
Variable::get_sizes_to_get_per_block(unsigned int transaction_id, const std::vector<size_t>& start,
                                     const std::vector<size_t>& count) const
{
  // Defensive check (should never trigger due to earlier validation)
  xbt_assert(start.size() == count.size() && start.size() == shape_.size(),
             "Internal error: dimension mismatch in get_sizes_to_get_per_block");

  std::vector<std::pair<std::string, sg_size_t>> get_sizes_per_block;
  // Validate transaction_id is within valid range (defense-in-depth: Transport also checks this)
  if (transaction_id > metadata_->get_current_transaction())                              // LCOV_EXCL_LINE
    throw InvalidTransactionIdException(XBT_THROW_POINT, std::to_string(transaction_id)); // LCOV_EXCL_LINE

  auto blocks = metadata_->get_blocks_for_transaction(transaction_id);
  XBT_DEBUG("%zu block(s) to check for transaction %u", blocks.size(), transaction_id);
  // For each block, compute the intersection between the requested region [start, start+count)
  // and the available block region [block_start, block_start+block_count) in each dimension.
  // Two cases of overlap are checked:
  //   1. Block starts within or after the requested region: start <= block_start < start+count
  //   2. Request starts within the block: block_start < start < block_start+block_count
  // The size to retrieve is the product of intersection sizes across all dimensions.
  // If any dimension has no overlap, nothing is retrieved from this block.
  for (const auto& [block_info, location] : blocks) {
    size_t size_to_get              = element_size_;
    auto [block_start, block_count] = block_info;
    auto [where, actor]             = location;
    auto something_to_get           = std::vector<bool>(start.size(), false);
    // Determine whether some elements have to be retrieved from this particular block
    for (unsigned i = 0; i < start.size(); i++) {
      XBT_DEBUG("Subscriber %s checks Publisher %s", sg4::Actor::self()->get_cname(), actor->get_cname());
      XBT_DEBUG("Dimension %u: wanted [%zu, %zu] vs. in block [%zu, %zu]", i, start[i], count[i], block_start[i],
                block_count[i]);
      size_t size_in_dim = 0;
      bool element_found = false;
      if (start[i] <= block_start[i] && (block_start[i] - start[i]) <= count[i]) {
        size_in_dim   = std::min(block_count[i], count[i] - (block_start[i] - start[i]));
        element_found = true;
      }
      if (start[i] > block_start[i] && (start[i] - block_start[i]) <= block_count[i]) {
        size_in_dim   = std::min(count[i], block_count[i] - (start[i] - block_start[i]));
        element_found = true;
      }

      if (element_found && size_in_dim > 0) {
        something_to_get[i] = true;
        XBT_DEBUG("Multiply size to read by %zu elements", size_in_dim);
        size_to_get *= size_in_dim;
      }
    }

    if (std::all_of(something_to_get.begin(), something_to_get.end(), [](bool v) { return v; })) {
      XBT_DEBUG("Total size to read from %s: %zu)", where.c_str(), size_to_get);
      get_sizes_per_block.emplace_back(where, size_to_get);
    }
  }
  return get_sizes_per_block;
}
/// \endcond

} // namespace dtlmod
