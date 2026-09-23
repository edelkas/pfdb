#pragma once

#include <ostream>
#include <vector>

#include "io/field_set.hpp"
#include "pfdb/film.hpp"

namespace pfdb {

/// Write `films` as RFC 4180 CSV to `os`: a header row followed by one row per
/// film. Columns: id, title, original_title, year, runtime, genres,
/// imdb_rating, fa_rating, date_watched, favorite, my_rating. List columns
/// (genres) are joined with "; ". Fields containing a comma, quote, or newline
/// are quoted with internal quotes doubled; rows end with CRLF.
void write_csv(std::ostream& os, const std::vector<const Film*>& films);

/// As above, but the columns are `id` followed by the selected fields (in
/// canonical order), so exports can be tailored via `--fields`/`--preset`.
void write_csv(std::ostream& os, const std::vector<const Film*>& films,
               const io::FieldSet& selection);

}  // namespace pfdb
