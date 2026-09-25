#include "sources/imdb/imdb_parser.hpp"

#include <string>

#include <nlohmann/json.hpp>

namespace pfdb::sources::imdb {
namespace {

using nlohmann::json;

constexpr const char* kSourceId = "imdb";

// --- Defensive JSON navigation -------------------------------------------
// IMDb's JSON is riddled with nulls (missing runtime, no plot, ...). These
// helpers treat "absent", "null", and "wrong type" identically so the parser
// never throws on a merely-incomplete record.

const json* member(const json& j, const char* key) {
    if (!j.is_object()) {
        return nullptr;
    }
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return nullptr;
    }
    return &*it;
}

// Follow a chain of object keys, stopping at the first missing/null link.
template <typename... Keys>
const json* path(const json& j, const char* key, Keys... rest) {
    const json* next = member(j, key);
    if (next == nullptr) {
        return nullptr;
    }
    if constexpr (sizeof...(rest) == 0) {
        return next;
    } else {
        return path(*next, rest...);
    }
}

std::string str_or(const json* j, const std::string& fallback = "") {
    return (j != nullptr && j->is_string()) ? j->get<std::string>() : fallback;
}

std::optional<int> opt_int(const json* j) {
    if (j != nullptr && j->is_number()) {
        return j->get<int>();
    }
    return std::nullopt;
}

CreditRole role_from_category(const std::string& category) {
    // IMDb category labels are pluralized display strings ("Directors",
    // "Writers", "Stars"). Match on a prefix so both singular and plural work.
    auto starts_with = [&](const char* p) { return category.rfind(p, 0) == 0; };
    if (starts_with("Director")) {
        return CreditRole::Director;
    }
    if (starts_with("Writer")) {
        return CreditRole::Writer;
    }
    if (starts_with("Star") || starts_with("Actor") || starts_with("Cast")) {
        return CreditRole::Actor;
    }
    if (starts_with("Producer")) {
        return CreditRole::Producer;
    }
    if (starts_with("Composer")) {
        return CreditRole::Composer;
    }
    return CreditRole::Other;
}

}  // namespace

std::vector<SearchResult> parse_suggestions(std::string_view json_text) {
    json j = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        throw SourceError(SourceError::Kind::Parse,
                          "imdb: could not parse search response");
    }

    std::vector<SearchResult> results;
    const json* d = member(j, "d");
    if (d == nullptr || !d->is_array()) {
        return results;  // no suggestions
    }

    for (const auto& item : *d) {
        const std::string id = str_or(member(item, "id"));
        // Keep only title entries; skip person ("nm") and video ("vi") hits.
        if (id.rfind("tt", 0) != 0) {
            continue;
        }
        SearchResult r;
        r.source = kSourceId;
        r.external_id = id;
        r.title = str_or(member(item, "l"));
        r.year = opt_int(member(item, "y"));
        r.type = str_or(member(item, "q"));
        r.subtitle = str_or(member(item, "s"));
        r.image_url = str_or(path(item, "i", "imageUrl"));
        results.push_back(std::move(r));
    }
    return results;
}

Film parse_title(std::string_view json_text) {
    json j = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        throw SourceError(SourceError::Kind::Parse,
                          "imdb: could not parse title response");
    }
    if (member(j, "errors") != nullptr) {
        throw SourceError(SourceError::Kind::Parse,
                          "imdb: GraphQL returned errors (schema may have changed)");
    }

    const json* title = path(j, "data", "title");
    // A non-existent id still returns a title object, but with a null
    // titleText. Treat "no title text" as not-found.
    const json* title_text = title != nullptr ? path(*title, "titleText", "text") : nullptr;
    if (title == nullptr || title_text == nullptr) {
        throw SourceError(SourceError::Kind::NotFound, "imdb: no such title");
    }

    Film f;
    f.title = str_or(title_text);
    f.original_title = str_or(path(*title, "originalTitleText", "text"));
    f.year = opt_int(path(*title, "releaseYear", "year"));

    if (const json* secs = path(*title, "runtime", "seconds")) {
        if (secs->is_number()) {
            f.runtime_minutes = secs->get<int>() / 60;
        }
    }

    f.synopsis = str_or(path(*title, "plot", "plotText", "plainText"));

    if (const json* genres = path(*title, "genres", "genres")) {
        if (genres->is_array()) {
            for (const auto& g : *genres) {
                std::string name = str_or(member(g, "text"));
                if (!name.empty()) {
                    f.genres.push_back(std::move(name));
                }
            }
        }
    }

    if (const json* agg = path(*title, "ratingsSummary", "aggregateRating")) {
        if (agg->is_number()) {
            Rating r;
            r.source = kSourceId;
            r.value = agg->get<double>();
            r.scale = 10.0;
            if (const json* votes = path(*title, "ratingsSummary", "voteCount")) {
                if (votes->is_number()) {
                    r.votes = votes->get<std::int64_t>();
                }
            }
            f.ratings.push_back(std::move(r));
        }
    }

    int order = 0;
    if (const json* pcs = member(*title, "principalCredits")) {
        if (pcs->is_array()) {
            for (const auto& pc : *pcs) {
                const CreditRole role = role_from_category(str_or(path(pc, "category", "text")));
                const json* credits = member(pc, "credits");
                if (credits == nullptr || !credits->is_array()) {
                    continue;
                }
                for (const auto& c : *credits) {
                    std::string name = str_or(path(c, "name", "nameText", "text"));
                    if (name.empty()) {
                        continue;
                    }
                    Credit credit;
                    credit.person.name = std::move(name);
                    credit.role = role;
                    credit.order = order++;
                    f.credits.push_back(std::move(credit));
                }
            }
        }
    }

    // Record the source identity; the fetch layer stamps fetched_at.
    SourceRef ref;
    ref.source = kSourceId;
    ref.external_id = str_or(member(*title, "id"));
    f.source_refs.push_back(std::move(ref));

    return f;
}

std::string parse_primary_image(std::string_view json_text) {
    json j = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        return {};
    }
    return str_or(path(j, "data", "title", "primaryImage", "url"));
}

}  // namespace pfdb::sources::imdb
