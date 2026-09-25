#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "net/http_client.hpp"
#include "sources/source.hpp"

namespace pfdb::sources::boxofficemojo {

/// BoxOfficeMojo source: financials (budget + worldwide gross) scraped from the
/// title page. It keys on IMDb ids (boxofficemojo.com/title/<ttid>/), so a
/// `fetch` returns a Film carrying only budget/gross plus a `boxofficemojo`
/// source_ref. Search is not supported (BOM is looked up by id).
class BomSource : public ISource {
public:
    explicit BomSource(net::IHttpClient& http) : http_(http) {}

    std::string id() const override { return "boxofficemojo"; }
    std::string display_name() const override { return "BoxOfficeMojo"; }

    std::vector<SearchResult> search(std::string_view query) override;
    SourceFetch fetch(std::string_view external_id) override;

private:
    net::IHttpClient& http_;
};

}  // namespace pfdb::sources::boxofficemojo
