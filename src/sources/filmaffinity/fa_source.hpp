#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "net/http_client.hpp"
#include "sources/source.hpp"

namespace pfdb::sources::filmaffinity {

/// FilmAffinity source. Search and detail are HTML pages (no API), parsed by
/// fa_parser's pure functions. `fetch` retrieves both the film page and the
/// relations page and combines them into a SourceFetch.
class FaSource : public ISource {
public:
    explicit FaSource(net::IHttpClient& http) : http_(http) {}

    std::string id() const override { return "filmaffinity"; }
    std::string display_name() const override { return "FilmAffinity"; }

    std::vector<SearchResult> search(std::string_view query) override;
    SourceFetch fetch(std::string_view external_id) override;

private:
    net::IHttpClient& http_;
};

/// True if `id` is a syntactically valid FilmAffinity id (all digits).
bool is_valid_fa_id(std::string_view id);

}  // namespace pfdb::sources::filmaffinity
