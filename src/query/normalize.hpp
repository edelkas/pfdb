#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pfdb/types.hpp"
#include "query/expr.hpp"
#include "query/filter.hpp"

namespace pfdb::query {

/// Distinct (person id, name) pairs per name field, e.g. index["cast"] lists
/// every actor in the collection. Built from the collection's credits and used
/// to turn a name-based inclusion into concrete id filters.
using PeopleIndex = std::unordered_map<std::string, std::vector<std::pair<Id, std::string>>>;

/// Rewrite every `NameInclusionFilter` leaf (`cast has "John"`) into an OR of
/// `IdInFilter`s over the person ids whose name matches the query (reusing the
/// case-insensitive substring matcher). Newly created id filters are appended
/// to `filters`. A name that matches nobody becomes a constant-false leaf.
/// Returns the normalized expression; the input tree is left unchanged.
ExprPtr normalize(const ExprPtr& expr, std::vector<Filter>& filters,
                  const PeopleIndex& people);

}  // namespace pfdb::query
