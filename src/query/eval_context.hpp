#pragma once

#include <unordered_map>
#include <vector>

#include "pfdb/types.hpp"

namespace pfdb::query {

/// Extra data an inclusion filter may need beyond a single `Film`.
///
/// Relations and similarities are edges between collection films, not fields on
/// `Film`, so the `related_to` / `similar_to` inclusion fields read them from
/// these adjacency maps (film id -> connected film ids). Both are optional: a
/// null pointer is treated as "no edges", which keeps filters that don't touch
/// edges trivially testable without a full model.
struct EvalContext {
    const std::unordered_map<Id, std::vector<Id>>* related = nullptr;
    const std::unordered_map<Id, std::vector<Id>>* similar = nullptr;
};

}  // namespace pfdb::query
