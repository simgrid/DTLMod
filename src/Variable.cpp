/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
////////////////////////////////////////////
///////////// PUBLIC INTERFACE /////////////
////////////////////////////////////////////

/// The global size of a Variable corresponds to the product of the number of elements in each dimension of the shape
/// vector by the element size.
size_t Variable::get_global_size() const
{
  size_t total_size = element_size_;
  for (const auto& s : shape_)
    total_size *= s;
  return total_size;
}

/// The local size of a Variable corresponds to the product of the number of elements in each dimension of the count
/// vector by the element size. If variable was published to the DTL over multiple transactions, multiply the size by
/// the number of transactions.
size_t Variable::get_local_size() const
{
  size_t total_size = element_size_;
  auto issuer       = sg4::Actor::self();
  for (const auto& c : local_count_.at(issuer))
    total_size *= c;
  if (transaction_start_ >= 0)
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
  auto start_and_count = std::make_pair(local_start_[publisher], local_count_[publisher]);
  metadata_->transaction_infos_[transaction_id][start_and_count] = std::make_pair(location, publisher);
}

std::vector<std::pair<std::string, sg_size_t>> Variable::get_sizes_to_get_per_block(unsigned int transaction_id,
                                                                                    std::vector<size_t> start,
                                                                                    std::vector<size_t> count) const
{
  std::vector<std::pair<std::string, sg_size_t>> get_sizes_per_block;
  // TODO add sanity checks
  auto blocks = metadata_->transaction_infos_[transaction_id];
  XBT_DEBUG("%zu block(s) to check for transaction %u", blocks.size(), transaction_id);
  for (auto [block_info, location] : blocks) {
    size_t size_to_get              = element_size_;
    auto [block_start, block_count] = block_info;
    auto [where, actor]             = location;
    // Determine whether some elements have to be retrieved from this particular block
    for (unsigned i = 0; i < start.size(); i++) {
      XBT_DEBUG("Dimension %u: wanted [%zu, %zu] vs. in block [%zu, %zu]", i, start[i], count[i], block_start[i],
                block_count[i]);
      size_t size_in_dim = 0;
      if (start[i] <= block_start[i]) {
        if ((block_start[i] - start[i]) <= count[i])
          size_in_dim = std::min(block_count[i], count[i] - (block_start[i] - start[i]));
      } else {
        if ((start[i] - block_start[i]) <= block_count[i])
          size_in_dim = std::min(count[i], block_count[i] - (start[i] - block_start[i]));
      }
      XBT_DEBUG("Mutiply size to read by %zu elements", size_in_dim);
      size_to_get *= size_in_dim;
    }

    XBT_DEBUG("Total size to read from %s: %zu)", where.c_str(), size_to_get);
    if (size_to_get > 0)
      get_sizes_per_block.push_back({where, size_to_get});
  }
  return get_sizes_per_block;
}
/// \endcond

} // namespace dtlmod