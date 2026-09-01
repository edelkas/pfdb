#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "net/http_client.hpp"
#include "pfdb/film.hpp"
#include "sources/source.hpp"

namespace pfdb::sources::imdb {

/// IMDb source. Search uses the public suggestion endpoint; detail uses IMDb's
/// GraphQL backend (the same one imdb.com's own pages call). Both return JSON,
/// so there is no HTML parsing. The class owns only the fetch step; parsing is
/// delegated to imdb_parser's pure functions.
class ImdbSource : public ISource {
public:
    explicit ImdbSource(net::IHttpClient& http) : http_(http) {}

    std::string id() const override { return "imdb"; }
    std::string display_name() const override { return "IMDb"; }

    std::vector<SearchResult> search(std::string_view query) override;
    SourceFetch fetch(std::string_view external_id) override;

private:
    net::IHttpClient& http_;
};

/// True if `id` is a syntactically valid IMDb title id ("tt" followed by
/// digits). Exposed for the source to validate ids before embedding them in a
/// request (and for tests).
bool is_valid_title_id(std::string_view id);

}  // namespace pfdb::sources::imdb
