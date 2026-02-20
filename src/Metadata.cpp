/* Copyright (c) 2024-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fstream>

#include "dtlmod/Variable.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_metadata, dtlmod, "DTL logging about Metadata");

namespace dtlmod {
/// \cond EXCLUDE_FROM_DOCUMENTATION
void Metadata::add_transaction(unsigned int id,
                               const std::pair<std::vector<size_t>, std::vector<size_t>>& start_and_count,
                               const std::string& location, sg4::ActorPtr publisher)
{
  transaction_infos_[id][start_and_count] = std::make_pair(location, publisher);
}

static void write_block_entries(std::ofstream& ostream,
                                const std::map<std::pair<std::vector<size_t>, std::vector<size_t>>,
                                               std::pair<std::string, sg4::ActorPtr>, std::less<>>& transaction)
{
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

void Metadata::write_transaction_to_stream(unsigned int tx_id, std::ofstream& out)
{
  auto it = transaction_infos_.find(tx_id);
  if (it == transaction_infos_.end())
    return;
  XBT_DEBUG("  Transaction %u:", tx_id);
  out << "  Transaction " << tx_id << ":" << std::endl;
  write_block_entries(out, it->second);
  flushed_count_++;
  transaction_infos_.erase(it);
}

void Metadata::evict_transaction(unsigned int tx_id)
{
  transaction_infos_.erase(tx_id);
}

void Metadata::export_to_file(std::ofstream& ostream, const std::string& prog_file_path) const
{
  auto var = variable_.lock();
  xbt_assert(var, "Metadata::export_to_file called after its Variable has been destroyed");
  XBT_DEBUG("Variable %s:", var->get_cname());
  unsigned int total = flushed_count_ + static_cast<unsigned int>(transaction_infos_.size());
  ostream << var->get_element_size() << "\t" << var->get_cname() << "\t" << total;
  ostream << "*{";
  auto shape            = var->get_shape();
  const auto last_index = shape.size() - 1;
  for (unsigned int i = 0; i < last_index; i++)
    ostream << shape[i] << ",";
  ostream << shape[last_index] << "}" << std::endl;

  // Copy already-flushed entries from the per-variable prog file (if any)
  if (!prog_file_path.empty()) {
    std::ifstream prog(prog_file_path, std::ios::binary);
    ostream << prog.rdbuf();
  }

  // Write remaining in-memory entries
  for (const auto& [id, transaction] : transaction_infos_) {
    XBT_DEBUG("  Transaction %u:", id);
    ostream << "  Transaction " << id << ":" << std::endl;
    write_block_entries(ostream, transaction);
  }
}
/// \endcond

} // namespace dtlmod
