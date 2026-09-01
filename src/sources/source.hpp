#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "pfdb/film.hpp"

namespace pfdb::sources {

/// One hit from a source's search. Enough to show the user a pick-list and then
/// fetch the full film by `external_id`.
struct SearchResult {
    std::string source;       // e.g. "imdb"
    std::string external_id;  // e.g. "tt0083658"
    std::string title;
    std::optional<int> year;
    std::string type;      // source's own type label, e.g. "feature", "TV series"
    std::string subtitle;  // supplementary line, e.g. top-billed cast
    std::string image_url;
};

/// Raised by sources on failure. `kind` lets the CLI map to an exit code and
/// message without matching on strings.
class SourceError : public std::runtime_error {
public:
    enum class Kind { NotFound, Network, Parse };

    SourceError(Kind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    Kind kind() const noexcept { return kind_; }

private:
    Kind kind_;
};

/// A film-data source (IMDb, FilmAffinity, ...). This is the seam that keeps the
/// core independent of any specific website. Implementations own the fetch step
/// (network) but delegate the parse step to pure functions so parsing stays
/// testable offline.
class ISource {
public:
    virtual ~ISource() = default;

    /// Stable lowercase token, e.g. "imdb".
    virtual std::string id() const = 0;
    /// Human-readable name, e.g. "IMDb".
    virtual std::string display_name() const = 0;

    /// Search the source for `query`, returning candidate titles.
    virtual std::vector<SearchResult> search(std::string_view query) = 0;

    /// Fetch and parse the full film identified by `external_id`. Throws
    /// SourceError{NotFound} if the id has no title.
    virtual Film fetch(std::string_view external_id) = 0;
};

}  // namespace pfdb::sources
