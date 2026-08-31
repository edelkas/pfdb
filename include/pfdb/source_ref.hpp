#pragma once

#include <optional>
#include <string>

#include "pfdb/types.hpp"

namespace pfdb {

/// Links a film to its identity on an external source.
///
/// A film may carry several of these (one per source it was enriched from),
/// which is what lets PFDB combine IMDb, FilmAffinity, etc. into a single
/// record. `external_id` is the source's native id (e.g. IMDb "tt0083658").
struct SourceRef {
    /// Source token, e.g. "imdb", "filmaffinity".
    std::string source;
    /// The film's id on that source.
    std::string external_id;
    /// When we last fetched data from this source for this film (UTC).
    std::optional<UnixSeconds> fetched_at;

    friend bool operator==(const SourceRef&, const SourceRef&) = default;
};

}  // namespace pfdb
