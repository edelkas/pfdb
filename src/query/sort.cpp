#include "query/sort.hpp"

#include <algorithm>
#include <cctype>

#include "query/parser.hpp"  // QueryError
#include "query/text_util.hpp"

namespace pfdb::query {
namespace {

std::string_view trim(std::string_view s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (std::isspace(static_cast<unsigned char>(s[b])) != 0)) {
        ++b;
    }
    while (e > b && (std::isspace(static_cast<unsigned char>(s[e - 1])) != 0)) {
        --e;
    }
    return s.substr(b, e - b);
}

template <typename T>
int order(const T& a, const T& b) {
    if (a < b) {
        return -1;
    }
    return b < a ? 1 : 0;
}

/// Three-way compare of two films by one key. Negative => `a` sorts first.
/// Unset optional values always sort last, before the direction flip.
int compare_key(const Film& a, const Film& b, const SortKey& key) {
    switch (key.kind) {
        case FieldKind::Text: {
            const auto sa = extract_text(key.field, a).value_or("");
            const auto sb = extract_text(key.field, b).value_or("");
            const int r = order(to_lower_ascii(sa), to_lower_ascii(sb));
            return key.descending ? -r : r;
        }
        case FieldKind::Number: {
            const auto na = extract_number(key.field, a);
            const auto nb = extract_number(key.field, b);
            if (!na && !nb) return 0;
            if (!na) return 1;
            if (!nb) return -1;
            const int r = order(*na, *nb);
            return key.descending ? -r : r;
        }
        case FieldKind::Date: {
            const auto da = extract_date(key.field, a);
            const auto db = extract_date(key.field, b);
            if (!da && !db) return 0;
            if (!da) return 1;
            if (!db) return -1;
            const int r = order(*da, *db);
            return key.descending ? -r : r;
        }
        default:
            return 0;
    }
}

}  // namespace

std::vector<SortKey> parse_sort(std::string_view spec) {
    std::vector<SortKey> keys;
    std::size_t start = 0;
    while (start <= spec.size()) {
        std::size_t comma = spec.find(',', start);
        if (comma == std::string_view::npos) {
            comma = spec.size();
        }
        const std::string_view part = trim(spec.substr(start, comma - start));
        start = comma + 1;
        if (part.empty()) {
            continue;
        }

        std::string_view field_tok = part;
        bool descending = false;
        if (const std::size_t colon = part.find(':'); colon != std::string_view::npos) {
            field_tok = trim(part.substr(0, colon));
            const std::string dir = to_lower_ascii(trim(part.substr(colon + 1)));
            if (dir == "desc" || dir == "descending") {
                descending = true;
            } else if (dir != "asc" && dir != "ascending" && !dir.empty()) {
                throw QueryError("bad sort direction '" + dir + "' (use asc or desc)");
            }
        }

        const auto info = find_field(field_tok);
        if (!info) {
            throw QueryError("unknown sort field: '" + std::string(field_tok) + "'");
        }
        if (info->kind != FieldKind::Text && info->kind != FieldKind::Number &&
            info->kind != FieldKind::Date) {
            throw QueryError("field '" + info->name + "' is a list and cannot be sorted");
        }
        keys.push_back({info->name, info->kind, descending});
    }
    return keys;
}

void sort_films(std::vector<const Film*>& films, const std::vector<SortKey>& keys) {
    if (keys.empty()) {
        return;
    }
    std::stable_sort(films.begin(), films.end(), [&](const Film* a, const Film* b) {
        for (const auto& key : keys) {
            const int c = compare_key(*a, *b, key);
            if (c != 0) {
                return c < 0;
            }
        }
        return false;
    });
}

}  // namespace pfdb::query
