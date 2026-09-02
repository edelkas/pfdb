#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "pfdb/film.hpp"
#include "query/field.hpp"

namespace pfdb::query {

/// One sort key: a scalar field and a direction.
struct SortKey {
    std::string field;
    FieldKind kind = FieldKind::Text;
    bool descending = false;
};

/// Parse a sort spec: comma-separated `field[:dir]`, dir = `asc` (default) or
/// `desc`. Only scalar fields (Text/Number/Date) may be sorted. Throws
/// QueryError on an unknown field, a non-scalar field, or a bad direction.
std::vector<SortKey> parse_sort(std::string_view spec);

/// Stable-sort `films` by `keys` (earlier keys are more significant). Films with
/// an unset value for a key sort last, regardless of the key's direction.
void sort_films(std::vector<const Film*>& films, const std::vector<SortKey>& keys);

}  // namespace pfdb::query
