#pragma once

#include <vector>

#include "io/emdb/emdb_parser.hpp"
#include "io/field_set.hpp"
#include "pfdb/film.hpp"

namespace pfdb::io::emdb {

/// Map decoded EMDB data to PFDB films, populating **only** the selected fields.
/// The IMDb id (when selected and present) becomes a `source_ref`, which import
/// uses as the identity key and `pfdb update` later uses to redownload metadata.
std::vector<Film> to_films(const EmdbData& data, const FieldSet& selection);

}  // namespace pfdb::io::emdb
