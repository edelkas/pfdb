#include "sources/boxofficemojo/bom_source.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include "sources/boxofficemojo/bom_parser.hpp"

namespace pfdb::sources::boxofficemojo {
namespace {

// A browser-ish User-Agent; BoxOfficeMojo serves an empty page to bare clients.
net::Headers bom_headers() {
    return {{"User-Agent",
             "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
             "(KHTML, like Gecko) Chrome/124.0 Safari/537.36"},
            {"Accept-Language", "en-US,en;q=0.9"}};
}

bool is_valid_title_id(std::string_view id) {
    if (id.size() < 3 || id.substr(0, 2) != "tt") {
        return false;
    }
    for (char c : id.substr(2)) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

UnixSeconds now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

std::vector<SearchResult> BomSource::search(std::string_view /*query*/) {
    // BoxOfficeMojo is keyed by IMDb id; there is nothing to search here.
    return {};
}

SourceFetch BomSource::fetch(std::string_view external_id) {
    if (!is_valid_title_id(external_id)) {
        throw SourceError(SourceError::Kind::NotFound,
                          "boxofficemojo: '" + std::string(external_id) +
                              "' is not a valid IMDb title id (expected e.g. tt1856101)");
    }
    const std::string id(external_id);
    const std::string url = "https://www.boxofficemojo.com/title/" + id + "/";
    net::HttpResponse resp = http_.get(url, bom_headers());
    if (!resp.ok()) {
        throw SourceError(SourceError::Kind::Network,
                          "boxofficemojo: request failed (HTTP " +
                              std::to_string(resp.status) + ")");
    }

    const Financials fin = parse_financials(resp.body);

    Film film;
    film.budget = fin.budget;
    film.gross = fin.gross;
    SourceRef ref;
    ref.source = "boxofficemojo";
    ref.external_id = id;
    ref.fetched_at = now_unix();
    film.source_refs.push_back(std::move(ref));

    return SourceFetch{std::move(film), {}, {}, {}};
}

}  // namespace pfdb::sources::boxofficemojo
