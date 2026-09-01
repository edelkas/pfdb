#include "sources/filmaffinity/fa_parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <unordered_set>

#include "parse/html.hpp"

namespace pfdb::sources::filmaffinity {
namespace {

using parse::HtmlDocument;
using parse::Node;

constexpr const char* kSourceId = "filmaffinity";

std::string trim(std::string_view s) {
    const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    auto begin = std::find_if(s.begin(), s.end(), not_space);
    auto end = std::find_if(s.rbegin(), s.rend(), not_space).base();
    return (begin < end) ? std::string(begin, end) : std::string();
}

// Collapse internal runs of whitespace to single spaces and trim the ends.
std::string normalize_ws(std::string_view s) {
    std::string out;
    bool in_space = false;
    for (unsigned char c : s) {
        if (std::isspace(c) != 0) {
            in_space = true;
        } else {
            if (in_space && !out.empty()) {
                out.push_back(' ');
            }
            in_space = false;
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::optional<int> to_int(std::string_view s) {
    std::string t = trim(s);
    // Drop grouping characters FilmAffinity may include (e.g. "140.614").
    std::string digits;
    for (char c : t) {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            digits.push_back(c);
        }
    }
    if (digits.empty()) {
        return std::nullopt;
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (ec != std::errc()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> to_double(std::string_view s) {
    std::string t = trim(s);
    if (t.empty()) {
        return std::nullopt;
    }
    try {
        return std::stod(t);
    } catch (...) {
        return std::nullopt;
    }
}

// Extract the numeric id from a FilmAffinity film href (".../film358476.html").
std::optional<std::string> film_id_from_href(std::string_view href) {
    const auto pos = href.find("/film");
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t i = pos + 5;  // past "/film"
    std::string digits;
    while (i < href.size() && std::isdigit(static_cast<unsigned char>(href[i])) != 0) {
        digits.push_back(href[i]);
        ++i;
    }
    if (digits.empty()) {
        return std::nullopt;
    }
    return digits;
}

std::string node_text(const std::optional<Node>& node) {
    return node.has_value() ? normalize_ws(node->text()) : std::string();
}

}  // namespace

FilmParse parse_film(std::string_view html) {
    HtmlDocument doc(html);

    const std::string title = node_text(doc.select_first("#main-title [itemprop=name]"));
    if (title.empty()) {
        throw SourceError(SourceError::Kind::NotFound, "filmaffinity: no such title");
    }

    Film f;
    f.spanish_title = title;
    f.title = title;  // FA-only fallback; IMDb overrides during merge
    f.spanish_synopsis = node_text(doc.select_first("[itemprop=description]"));
    f.synopsis = f.spanish_synopsis;  // fallback

    // Rating / votes / review count live in clean content attributes.
    if (auto n = doc.select_first("[itemprop=ratingValue]")) {
        if (auto value = to_double(n->attr("content").value_or(""))) {
            Rating r;
            r.source = kSourceId;
            r.value = *value;
            r.scale = 10.0;
            if (auto votes = doc.select_first("[itemprop=ratingCount]")) {
                if (auto v = to_int(votes->attr("content").value_or(""))) {
                    r.votes = *v;
                }
            }
            f.ratings.push_back(std::move(r));
        }
    }
    if (auto n = doc.select_first("[itemprop=reviewCount]")) {
        f.review_count = to_int(n->attr("content").value_or(""));
    }

    // Topics ("temas") and groups: distinct link targets. Attribute values with
    // '.' must be quoted or they are not a valid CSS selector.
    for (const auto& a : doc.select("a[href*=\"movietopic.php\"]")) {
        std::string t = normalize_ws(a.text());
        if (!t.empty()) {
            f.topics.push_back(std::move(t));
        }
    }
    std::unordered_set<std::string> seen_groups;
    for (const auto& a : doc.select("a[href*=\"movie-group.php\"]")) {
        std::string g = normalize_ws(a.text());
        if (!g.empty() && seen_groups.insert(g).second) {
            f.groups.push_back(std::move(g));
        }
    }

    // Similar movies: li.slider-item with a similarity chart.
    FilmParse result;
    for (const auto& li : doc.select("li.slider-item[data-movie-id]")) {
        auto id = li.attr("data-movie-id");
        auto anim = li.select_first("animate");
        if (!id.has_value() || !anim.has_value()) {
            continue;  // not a similarity slider entry
        }
        auto pct = to_int(anim->attr("to").value_or(""));
        if (!pct.has_value()) {
            continue;
        }
        result.similars.push_back(SimilarRef{*id, *pct});
    }

    SourceRef ref;
    ref.source = kSourceId;
    ref.external_id = "";  // filled by the source (it knows the requested id)
    f.source_refs.push_back(std::move(ref));

    result.film = std::move(f);
    return result;
}

std::vector<RelatedRef> parse_relations(std::string_view html) {
    HtmlDocument doc(html);
    std::vector<RelatedRef> refs;

    for (const auto& card : doc.select("div.fa-content-card")) {
        std::string kind = node_text(card.select_first("div.card-header"));
        // Normalize the label: drop a trailing colon.
        if (!kind.empty() && kind.back() == ':') {
            kind.pop_back();
        }
        kind = trim(kind);

        for (const auto& movie : card.select("[data-movie-id]")) {
            auto id = movie.attr("data-movie-id");
            if (id.has_value() && !id->empty()) {
                refs.push_back(RelatedRef{*id, kind});
            }
        }
    }
    return refs;
}

std::vector<SearchResult> parse_search(std::string_view html) {
    HtmlDocument doc(html);
    std::vector<SearchResult> results;
    std::unordered_set<std::string> seen;

    // Results are grouped by year; each group carries a year header.
    for (const auto& group : doc.select("li.se-it")) {
        std::optional<int> year = to_int(node_text(group.select_first(".group-by.year")));
        for (const auto& a : group.select("a[href*=\"film\"]")) {
            auto href = a.attr("href");
            std::string title = normalize_ws(a.text());
            if (!href.has_value() || title.empty()) {
                continue;
            }
            auto id = film_id_from_href(*href);
            if (!id.has_value() || !seen.insert(*id).second) {
                continue;
            }
            SearchResult r;
            r.source = kSourceId;
            r.external_id = *id;
            r.title = std::move(title);
            r.year = year;
            results.push_back(std::move(r));
        }
    }

    // A single strong match redirects straight to the film page. Recover one
    // result from it so `search` behaves consistently.
    if (results.empty()) {
        std::string title = node_text(doc.select_first("#main-title [itemprop=name]"));
        auto og = doc.select_first("meta[property=\"og:url\"]");
        auto href = og.has_value() ? og->attr("content") : std::nullopt;
        auto id = href.has_value() ? film_id_from_href(*href) : std::nullopt;
        if (!title.empty() && id.has_value()) {
            SearchResult r;
            r.source = kSourceId;
            r.external_id = *id;
            r.title = std::move(title);
            results.push_back(std::move(r));
        }
    }
    return results;
}

}  // namespace pfdb::sources::filmaffinity
