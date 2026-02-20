/* Copyright (c) 2024-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_METADATA_HPP__
#define __DTLMOD_METADATA_HPP__

#include <fstream>

#include <fsmod/File.hpp>
#include <simgrid/s4u/Actor.hpp>

namespace sg4 = simgrid::s4u;

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

class Variable;

/// \cond EXCLUDE_FROM_DOCUMENTATION
class Metadata {
  friend Variable;
  std::weak_ptr<Variable> variable_;

  std::map<unsigned int,                                                 // Transaction id
           std::map<std::pair<std::vector<size_t>, std::vector<size_t>>, // starts and counts
                    std::pair<std::string, sg4::ActorPtr>,               // filename and publisher
                    std::less<>>,
           std::less<>>
      transaction_infos_;

  unsigned int flushed_count_ = 0; // number of transactions already flushed to the prog file

protected:
  const std::map<std::pair<std::vector<size_t>, std::vector<size_t>>, std::pair<std::string, sg4::ActorPtr>,
                 std::less<>>&
  get_blocks_for_transaction(unsigned int id)
  {
    return transaction_infos_[id];
  }
  void add_transaction(unsigned int id, const std::pair<std::vector<size_t>, std::vector<size_t>>& start_and_count,
                       const std::string& filename, sg4::ActorPtr publisher);

public:
  explicit Metadata(const std::shared_ptr<Variable>& variable) noexcept : variable_(variable) {}
  unsigned int get_current_transaction() const noexcept
  {
    return transaction_infos_.empty() ? 0 : (transaction_infos_.rbegin())->first;
  }
  // Write entries for tx_id to out, increment flushed_count_, erase from transaction_infos_
  void write_transaction_to_stream(unsigned int tx_id, std::ofstream& out);
  // Remove tx_id from transaction_infos_ without writing to file
  void evict_transaction(unsigned int tx_id);
  // Write all remaining transactions; prog_file_path contains already-flushed entries to prepend
  void export_to_file(std::ofstream& ostream, const std::string& prog_file_path = "") const;
};
/// \endcond

} // namespace dtlmod
#endif
