#pragma once

#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <variant>

#include "pfdb/film.hpp"
#include "pfdb/types.hpp"
#include "query/eval_context.hpp"

namespace pfdb::query {

/// A textual match against a single text field. `literal` is the fast path
/// (case-insensitive substring); when `regex` is set, `re` is the compiled
/// pattern (compiled once at parse time and shared).
struct TextFilter {
    std::string field;
    bool regex = false;
    std::string literal;
    std::shared_ptr<std::regex> re;
};

/// An inclusive/exclusive numeric range. Unset bounds mean "open on that side";
/// equal inclusive bounds make this an equality test.
struct NumberFilter {
    std::string field;
    std::optional<double> min;
    std::optional<double> max;
    bool min_inclusive = true;
    bool max_inclusive = true;
};

/// A date range, compared lexicographically over ISO "YYYY-MM-DD" strings
/// (which orders chronologically).
struct DateFilter {
    std::string field;
    std::optional<std::string> min;
    std::optional<std::string> max;
    bool min_inclusive = true;
    bool max_inclusive = true;
};

/// Exact (case-insensitive) membership in a string list (genre/topic/group).
struct StringInFilter {
    std::string field;
    std::string value;
};

/// Exact membership in an id list (cast_id/related_to/...).
struct IdInFilter {
    std::string field;
    Id value = kInvalidId;
};

/// A name-based inclusion (cast/director/writer) awaiting normalization: the
/// query engine rewrites it into an OR of `IdInFilter`s over the matching
/// person ids. It never reaches evaluation directly.
struct NameInclusionFilter {
    std::string name_field;  // "cast" / "director" / "writer"
    std::string query;       // the name substring the user typed
};

using Filter = std::variant<TextFilter, NumberFilter, DateFilter, StringInFilter,
                            IdInFilter, NameInclusionFilter>;

/// Does `film` satisfy `filter`? Pure; `ctx` supplies edge data for the
/// related_to/similar_to id fields (may be empty for other filters).
bool matches(const Filter& filter, const Film& film, const EvalContext& ctx);

}  // namespace pfdb::query
