#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include "query/expr.hpp"
#include "query/filter.hpp"

namespace pfdb::query {

/// Thrown for any malformed filter spec or boolean expression. The CLI maps it
/// to a usage error.
class QueryError : public std::runtime_error {
public:
    explicit QueryError(const std::string& message) : std::runtime_error(message) {}
};

/// Parse a single filter spec ("FIELD OP VALUE", e.g. `year >= 1980`,
/// `title ~ blade`, `cast has "Harrison Ford"`). Throws QueryError.
Filter parse_filter(std::string_view spec);

/// Parse a boolean expression over the leaf identifiers `F1..F<count>`
/// (case-insensitive), e.g. `F1 AND (F2 OR F3)`. An empty string is rejected;
/// callers use `default_expr` for the no-expression case. Throws QueryError.
ExprPtr parse_where(std::string_view expr, std::size_t filter_count);

}  // namespace pfdb::query
