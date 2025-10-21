/* Copyright (c) 2024-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fstream>

#include "dtlmod/Variable.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(dtlmod);

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION
void Metadata::add_transaction(int id, const std::pair<std::vector<size_t>, std::vector<size_t>>& start_and_count,
                               const std::string& location, sg4::ActorPtr publisher)
{
  transaction_infos_[id][start_and_count] = std::make_pair(location, publisher);
}

void Metadata::export_to_file(std::ofstream& ostream) const
{
  XBT_DEBUG("Variable %s:", variable_->get_cname());

  ostream << variable_->get_element_size() << "\t" << variable_->get_cname() << "\t" << transaction_infos_.size();
  ostream << "*{";
  auto shape = variable_->get_shape();
  for (unsigned int i = 0; i < shape.size() - 1; i++)
    ostream << shape[i] << ",";
  ostream << shape[shape.size() - 1] << "}" << std::endl;

  for (const auto& [id, transaction] : transaction_infos_) {
    XBT_DEBUG("  Transaction %u:", id);
    ostream << "  Transaction " << id << ":" << std::endl;
    for (const auto& [block_info, location] : transaction) {
      const auto& [block_start, block_count] = block_info;
      const auto& [where, actor]             = location;

      ostream << "    " << where.c_str() << ": [";
      XBT_DEBUG("    Actor %s wrote:", actor->get_cname());
      unsigned long last = block_start.size() - 1;
      for (unsigned long i = 0; i < last; i++) {
        ostream << block_start[i] << ":" << block_start[i] + block_count[i] << ", ";
        XBT_DEBUG("      Dimension %lu : [%zu..%zu]", i + 1, block_start[i], block_start[i] + block_count[i]);
      }
      ostream << block_start[last] << ":" << block_start[last] + block_count[last] << "]" << std::endl;
      XBT_DEBUG("      Dimension %lu : [%zu..%zu]", last + 1, block_start[last], block_start[last] + block_count[last]);

      XBT_DEBUG("    in: %s", where.c_str());
    }
  }
}
/// \endcond

} // namespace dtlmod
