#pragma once

#include <string_view>
#include <vector>

#include "pfdb/film.hpp"
#include "sources/source.hpp"

namespace pfdb::sources::filmaffinity {

/// Result of parsing a FilmAffinity film page: the film plus the "similar
/// movies" edges listed on that page. Relations come from a separate page.
struct FilmParse {
    Film film;
    std::vector<SimilarRef> similars;
};

/// Parse a FilmAffinity search results page into candidate titles.
std::vector<SearchResult> parse_search(std::string_view html);

/// Parse a FilmAffinity film page (film<ID>.html). Sets the FA-specific fields
/// (spanish_title/synopsis, fa rating + votes + review_count, topics, groups)
/// and, as a fallback for FA-only films, `title`/`synopsis`. The film's
/// source_ref carries the FA id but no fetched_at. Throws SourceError{NotFound}
/// when the page has no title.
FilmParse parse_film(std::string_view html);

/// Parse a FilmAffinity relations page (movie-relations.php). Each returned ref
/// carries the related film's FA id and the Spanish relationship label from its
/// group header (e.g. "tiene secuela").
std::vector<RelatedRef> parse_relations(std::string_view html);

}  // namespace pfdb::sources::filmaffinity
