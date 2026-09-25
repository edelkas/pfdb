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

/// A film related to the fetched one, identified by the source's own id. The
/// `kind` is the source's relationship label (e.g. FilmAffinity's "tiene
/// secuela"); empty when the source expresses no type.
struct RelatedRef {
    std::string external_id;
    std::string kind;
};

/// A film similar to the fetched one, with the source's similarity percentage.
struct SimilarRef {
    std::string external_id;
    int percent = 0;
};

/// Everything a source returns for one title: the film itself plus film-to-film
/// edges (relations, similarities) the source knows about. Edge targets are the
/// *source's* ids; resolving them to collection films (and keeping only pairs
/// already in the database) happens in the app/persistence layer.
struct SourceFetch {
    Film film;
    std::vector<RelatedRef> relations;
    std::vector<SimilarRef> similars;
    /// URL of the film's cover/poster image, when the source exposes one. The app
    /// downloads and stores it (as a blob) only when the user asks for a cover.
    std::string cover_url;
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

    /// Fetch and parse the full film identified by `external_id`, along with any
    /// relation/similarity edges the source exposes. Throws SourceError{NotFound}
    /// if the id has no title.
    virtual SourceFetch fetch(std::string_view external_id) = 0;
};

}  // namespace pfdb::sources
