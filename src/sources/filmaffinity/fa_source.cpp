#include "sources/filmaffinity/fa_source.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include "sources/filmaffinity/fa_parser.hpp"

namespace pfdb::sources::filmaffinity {
namespace {

constexpr const char* kBase = "https://www.filmaffinity.com/es";

// FilmAffinity returns Spanish content and expects a browser-ish request.
net::Headers fa_headers() {
    return {{"Accept-Language", "es-ES,es;q=0.9,en;q=0.8"}};
}

std::string url_encode(std::string_view s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

UnixSeconds now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

bool is_valid_fa_id(std::string_view id) {
    if (id.empty()) {
        return false;
    }
    for (char c : id) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

std::vector<SearchResult> FaSource::search(std::string_view query) {
    const std::string url =
        std::string(kBase) + "/search.php?stext=" + url_encode(query);
    net::HttpResponse resp = http_.get(url, fa_headers());
    if (!resp.ok()) {
        throw SourceError(SourceError::Kind::Network,
                          "filmaffinity: search request failed (HTTP " +
                              std::to_string(resp.status) + ")");
    }
    return parse_search(resp.body);
}

SourceFetch FaSource::fetch(std::string_view external_id) {
    if (!is_valid_fa_id(external_id)) {
        throw SourceError(SourceError::Kind::NotFound,
                          "filmaffinity: '" + std::string(external_id) +
                              "' is not a valid FilmAffinity id (expected digits)");
    }
    const std::string id(external_id);

    // Film page.
    net::HttpResponse film_resp =
        http_.get(std::string(kBase) + "/film" + id + ".html", fa_headers());
    if (!film_resp.ok()) {
        throw SourceError(SourceError::Kind::Network,
                          "filmaffinity: film request failed (HTTP " +
                              std::to_string(film_resp.status) + ")");
    }
    FilmParse parsed = parse_film(film_resp.body);

    SourceFetch out;
    out.film = std::move(parsed.film);
    // Stamp our source ref with the requested id and fetch time.
    for (auto& ref : out.film.source_refs) {
        if (ref.source == "filmaffinity") {
            ref.external_id = id;
            ref.fetched_at = now_unix();
        }
    }
    // Similar edges (drop any self-reference defensively).
    for (auto& s : parsed.similars) {
        if (s.external_id != id) {
            out.similars.push_back(std::move(s));
        }
    }

    // Relations page (separate request).
    net::HttpResponse rel_resp = http_.get(
        std::string(kBase) + "/movie-relations.php?movie-id=" + id, fa_headers());
    if (rel_resp.ok()) {
        for (auto& r : parse_relations(rel_resp.body)) {
            if (r.external_id != id) {
                out.relations.push_back(std::move(r));
            }
        }
    }
    return out;
}

}  // namespace pfdb::sources::filmaffinity
