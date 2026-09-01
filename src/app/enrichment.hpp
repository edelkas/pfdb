#pragma once

#include <optional>
#include <string>
#include <vector>

#include "pfdb/film.hpp"

namespace pfdb::app {

/// User-supplied data layered on top of a fetched film. These are the manual
/// flags of `pfdb add` (favourite, date watched, ...) that a source can't know.
struct FilmOverrides {
    bool favorite = false;
    std::optional<std::string> date_watched;
    std::optional<double> personal_rating;
    std::optional<std::string> notes;
    std::vector<std::string> add_genres;   // appended to the fetched genres
    std::optional<std::string> title;      // override the fetched title
    std::optional<int> year;               // override the fetched year
};

/// Apply `ov` onto `film` in place. Unset optionals leave the fetched value
/// untouched; `add_genres` are appended (de-duplicated).
///
/// This is the enrichment seam. In M2 it only layers the user's own data onto a
/// single source's result; multi-source merging with provenance arrives in M3.
void apply_overrides(Film& film, const FilmOverrides& ov);

}  // namespace pfdb::app
