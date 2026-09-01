#include "sources/imdb/imdb_source.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "sources/imdb/imdb_parser.hpp"

namespace pfdb::sources::imdb {
namespace {

constexpr const char* kSuggestionHost = "https://v3.sg.media-imdb.com";
constexpr const char* kGraphqlUrl = "https://caching.graphql.imdb.com/";

// The single GraphQL query for title detail. Keep byte-for-byte in sync with
// TITLE_QUERY in tools/capture_imdb_fixtures.py so fixtures match production.
constexpr const char* kTitleQuery =
    "{title(id:\"%ID%\"){id titleText{text} originalTitleText{text} "
    "titleType{text} releaseYear{year} runtime{seconds} "
    "ratingsSummary{aggregateRating voteCount} plot{plotText{plainText}} "
    "genres{genres{text}} primaryImage{url} "
    "principalCredits{category{text} credits{name{id nameText{text}}}}}}";

// Headers IMDb's GraphQL backend requires; without Origin/Referer it 403s.
net::Headers graphql_headers() {
    return {
        {"Content-Type", "application/json"},
        {"Origin", "https://www.imdb.com"},
        {"Referer", "https://www.imdb.com/"},
    };
}

// Percent-encode a query for use in the suggestion URL path. Encodes anything
// that isn't an unreserved URL character.
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

std::string build_title_query(std::string_view id) {
    std::string q = kTitleQuery;
    const std::string placeholder = "%ID%";
    const auto pos = q.find(placeholder);
    q.replace(pos, placeholder.size(), id);
    return q;
}

}  // namespace

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

std::vector<SearchResult> ImdbSource::search(std::string_view query) {
    const std::string url = std::string(kSuggestionHost) + "/suggestion/x/" +
                            url_encode(query) + ".json?includeVideos=0";
    net::HttpResponse resp = http_.get(url);
    if (!resp.ok()) {
        throw SourceError(SourceError::Kind::Network,
                          "imdb: search request failed (HTTP " +
                              std::to_string(resp.status) + ")");
    }
    return parse_suggestions(resp.body);
}

Film ImdbSource::fetch(std::string_view external_id) {
    if (!is_valid_title_id(external_id)) {
        throw SourceError(SourceError::Kind::NotFound,
                          "imdb: '" + std::string(external_id) +
                              "' is not a valid IMDb title id (expected e.g. tt0083658)");
    }
    const nlohmann::json body = {{"query", build_title_query(external_id)}};
    net::HttpResponse resp = http_.post(kGraphqlUrl, body.dump(), graphql_headers());
    if (!resp.ok()) {
        throw SourceError(SourceError::Kind::Network,
                          "imdb: title request failed (HTTP " +
                              std::to_string(resp.status) + ")");
    }

    Film film = parse_title(resp.body);
    // Stamp the fetch time on the IMDb source reference (parser left it unset).
    for (auto& ref : film.source_refs) {
        if (ref.source == id()) {
            ref.fetched_at = now_unix();
        }
    }
    return film;
}

}  // namespace pfdb::sources::imdb
