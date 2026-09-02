#pragma once

#include <string>
#include <vector>

#include "model/collection_model.hpp"
#include "pfdb/film.hpp"
#include "query/normalize.hpp"

namespace pfdb::query {

/// A whole query as the user expressed it on the command line.
struct QueryRequest {
    std::vector<std::string> filter_specs;  ///< Each `--filter` spec (F1, F2, ...).
    std::string where;                       ///< `--where` expression ("" = AND all).
    std::string sort;                        ///< `--sort` spec ("" = load order).
};

/// Build the people index (name -> ids per role) used to normalize name-based
/// inclusion filters, from the collection's credits.
PeopleIndex build_people_index(const CollectionModel& model);

/// Parse, normalize, and run `req` against `model`, returning the matching films
/// in the requested order (load order when no sort is given). Throws QueryError
/// on any malformed spec/expression/sort.
std::vector<const Film*> run_query(const CollectionModel& model, const QueryRequest& req);

}  // namespace pfdb::query
