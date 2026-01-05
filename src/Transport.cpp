/* Copyright (c) 2024-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/MessageQueue.hpp>

#include "dtlmod/DTLException.hpp"
#include "dtlmod/Transport.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_transport, dtlmod_engine, "DTL logging about Transport");

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION

std::vector<std::pair<std::string, sg_size_t>>
Transport::check_selection_and_get_blocks_to_get(std::shared_ptr<Variable> var) const
{
  auto self = sg4::Actor::self();
  // If the actor made no selection, get the full variable, ie. use a vector full of zeros as start and the global
  // shape of the variable as count.
  auto start = std::vector<size_t>(var->get_shape().size(), 0);

  std::vector<size_t> count;
  if (var->is_reduced_by_subscriber())
    count = var->get_reduction_method()->get_reduced_variable_shape(var);
  else
    count = var->get_shape();

  // If the actor made no transaction selection, get the last one
  unsigned int transaction_start = var->get_metadata()->get_current_transaction();
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
  var->set_local_start_and_count(self, std::make_pair(start, count));

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
