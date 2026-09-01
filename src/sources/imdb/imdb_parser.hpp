#pragma once

#include <string_view>
#include <vector>

#include "pfdb/film.hpp"
#include "sources/source.hpp"

namespace pfdb::sources::imdb {

/// Parse a response from IMDb's suggestion (search) endpoint
/// (v3.sg.media-imdb.com/suggestion/x/{query}.json) into search results.
/// Only title entries (ids beginning "tt") are returned; person and video
/// suggestions are skipped. Throws SourceError{Parse} on malformed JSON.
std::vector<SearchResult> parse_suggestions(std::string_view json);

/// Parse a response from IMDb's GraphQL title query (see kTitleQuery in
/// imdb_source.cpp) into a Film. The film's source_ref carries the IMDb id but
/// no fetched_at (a pure parse has no clock); the fetching layer stamps that.
/// Throws SourceError{NotFound} when the id resolves to no real title, and
/// SourceError{Parse} on malformed JSON or a GraphQL error response.
Film parse_title(std::string_view json);

}  // namespace pfdb::sources::imdb
