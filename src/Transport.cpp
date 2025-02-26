/* Copyright (c) 2024-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTLException.hpp"
#include "dtlmod/Transport.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

void Transport::set_publisher_put_requests_mq(const std::string& publisher_name)
{
  publisher_put_requests_mq_[publisher_name] = sg4::MessageQueue::by_name(publisher_name);
}

sg4::MessageQueue* Transport::get_publisher_put_requests_mq(const std::string& publisher_name) const
{
  return publisher_put_requests_mq_.at(publisher_name);
}

std::vector<std::pair<std::string, sg_size_t>>
Transport::check_selection_and_get_blocks_to_get(std::shared_ptr<Variable> var)
{
  auto self = sg4::Actor::self();
  // If the actor made no selection, get the full variable, ie. use a vector full of zeros as start and the global
  // shape of the variable as count.
  std::vector<size_t> start = std::vector<size_t>(var->get_shape().size(), 0);
  std::vector<size_t> count = var->get_shape();
  // If the actor made no transaction selection, get the last one
  int transaction_start          = var->get_metadata()->get_current_transaction();
  unsigned int transaction_count = 1;

  // Check if a selection has been made by this actor, update start and count accordingly if it is the case.
  if (var->subscriber_has_a_selection(self)) {
    XBT_DEBUG("Actor %s made a selection for Variable %s", self->get_cname(), var->get_cname());
    std::tie(start, count) = var->get_subscriber_selection(self);
  }

  // Check if a transaction selection has been made by this actor, update transaction_id and transaction_count
  // accordingly if it is the case.
  if (var->subscriber_has_a_transaction_selection(self)) {
    XBT_DEBUG("Actor %s made a transaction selection for Variable %s", self->get_cname(), var->get_cname());
    std::tie(transaction_start, transaction_count) = var->get_subscriber_transaction_selection(self);
  }

  if ((transaction_start + transaction_count - 1) > var->get_metadata()->get_current_transaction())
    throw GetWhenNoTransactionException(XBT_THROW_POINT, var->get_name());

  // Store the local count and start for 'var' on this actor
  var->set_local_start(self, start);
  var->set_local_count(self, count);

  // Update the number of stored transactions. Every transaction reset this information
  var->set_transaction_start(std::min(transaction_start, var->get_metadata()->get_current_transaction()));
  var->set_transaction_count(transaction_count);

  // Determine what data blocks to read for each requested transaction
  auto blocks = var->get_sizes_to_get_per_block(transaction_start, start, count);
  for (unsigned int i = 1; i < transaction_count; i++) {
    auto extra_block = var->get_sizes_to_get_per_block(transaction_start + i, start, count);
    blocks.insert(blocks.end(), extra_block.begin(), extra_block.end());
  }
  return blocks;
}
/// \endcond

} // namespace dtlmod
