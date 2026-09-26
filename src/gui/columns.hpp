#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "pfdb/film.hpp"

namespace pfdb::gui {

/// A selectable table column. `token` is the query field token, which doubles
/// as the sort key, so header-click sorting maps straight onto our sort specs.
struct Column {
    std::string token;
    std::string header;
    bool default_visible = false;
    bool sortable = false;  // scalar fields only
    bool numeric = false;   // right-align
};

/// The full, ordered set of columns offered by the table (defaults first).
const std::vector<Column>& all_columns();

/// A human-readable value for a field token on a film ("" when absent). Lists
/// (genres, directors, ...) are comma-joined; ratings show one decimal.
std::string format_field(std::string_view token, const Film& film);

}  // namespace pfdb::gui
