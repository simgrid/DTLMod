/* Copyright (c) 2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_METADATA_HPP__
#define __DTLMOD_METADATA_HPP__

#include <fsmod/File.hpp>
#include <simgrid/s4u/Actor.hpp>

namespace sg4 = simgrid::s4u;

XBT_LOG_EXTERNAL_CATEGORY(dtlmod);

namespace dtlmod {

class Variable;

/// \cond EXCLUDE_FROM_DOCUMENTATION
class Metadata {
  Variable* variable_;

public:
  explicit Metadata(Variable* variable) : variable_(variable) {}

  std::map<unsigned int,                                                 // Transaction id
           std::map<std::pair<std::vector<size_t>, std::vector<size_t>>, // starts and counts
                    std::pair<std::string, sg4::ActorPtr>,               // filename and publisher
                    std::less<>>,
           std::less<>>
      transaction_infos_;

  int get_current_transaction() const { return transaction_infos_.empty() ? -1 : (transaction_infos_.rbegin())->first; }
  void export_to_file(std::ofstream& ostream) const;
};
/// \endcond

} // namespace dtlmod
#endif
