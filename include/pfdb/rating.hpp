#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "pfdb/types.hpp"

namespace pfdb {

/// A score for a film as reported by one source.
///
/// `value` and `scale` are stored verbatim as the source reports them so we
/// never lose precision to normalization (IMDb uses /10, others /100, /5,
/// etc.). Presentation-layer normalization is a display concern, not a
/// storage one.
struct Rating {
    /// Source token, e.g. "imdb", "filmaffinity", "user".
    std::string source;
    /// The score itself, on `scale` (e.g. 8.1 on a scale of 10).
    double value = 0.0;
    /// Maximum of the scale the score is expressed on (e.g. 10, 100, 5).
    double scale = 10.0;
    /// Number of votes behind the score, when the source exposes it.
    std::optional<std::int64_t> votes;

    friend bool operator==(const Rating&, const Rating&) = default;
};

}  // namespace pfdb
