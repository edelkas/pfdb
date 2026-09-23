#include "io/field_set.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "app/config.hpp"

namespace pfdb::io {
namespace {

struct Entry {
    Field field;
    std::string_view token;
};

// Canonical order + tokens for every field. Order here defines iteration order.
constexpr std::array<Entry, 22> kEntries = {{
    {Field::Title, "title"},
    {Field::OriginalTitle, "original-title"},
    {Field::Year, "year"},
    {Field::Runtime, "runtime"},
    {Field::Synopsis, "synopsis"},
    {Field::Genres, "genres"},
    {Field::Cast, "cast"},
    {Field::Directors, "directors"},
    {Field::Writers, "writers"},
    {Field::Composers, "composers"},
    {Field::Topics, "topics"},
    {Field::Groups, "groups"},
    {Field::ImdbId, "imdb-id"},
    {Field::ImdbRating, "imdb-rating"},
    {Field::UserRating, "user-rating"},
    {Field::WatchDate, "watch-date"},
    {Field::WatchCount, "watch-count"},
    {Field::Owned, "owned"},
    {Field::Wishlist, "wishlist"},
    {Field::Favorite, "favorite"},
    {Field::Comments, "comments"},
    {Field::VideoFile, "video-file"},
}};

// The user-specific fields (used to split `userdata` from `metadata`).
bool is_user_field(Field f) {
    switch (f) {
        case Field::UserRating:
        case Field::WatchDate:
        case Field::WatchCount:
        case Field::Owned:
        case Field::Wishlist:
        case Field::Favorite:
        case Field::Comments:
            return true;
        default:
            return false;
    }
}

std::string normalize(std::string_view token) {
    std::string s;
    s.reserve(token.size());
    for (char c : token) {
        if (c == '_') {
            c = '-';
        }
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    // trim surrounding spaces
    const auto b = s.find_first_not_of(' ');
    const auto e = s.find_last_not_of(' ');
    if (b == std::string::npos) {
        return {};
    }
    return s.substr(b, e - b + 1);
}

}  // namespace

std::size_t FieldSet::kCount() { return kEntries.size(); }

void FieldSet::add(Field f) { bits_[static_cast<std::size_t>(f)] = true; }
bool FieldSet::contains(Field f) const { return bits_[static_cast<std::size_t>(f)]; }
bool FieldSet::empty() const {
    return std::none_of(bits_.begin(), bits_.end(), [](bool b) { return b; });
}

std::vector<Field> FieldSet::fields() const {
    std::vector<Field> out;
    for (const auto& e : kEntries) {
        if (contains(e.field)) {
            out.push_back(e.field);
        }
    }
    return out;
}

std::vector<std::string> FieldSet::tokens() const {
    std::vector<std::string> out;
    for (Field f : fields()) {
        out.emplace_back(token_of(f));
    }
    return out;
}

FieldSet FieldSet::all() {
    FieldSet s;
    for (const auto& e : kEntries) {
        s.add(e.field);
    }
    return s;
}

FieldSet FieldSet::userdata() {
    FieldSet s;
    // Identity anchor + display, then the user fields.
    s.add(Field::ImdbId);
    s.add(Field::Title);
    s.add(Field::Year);
    for (const auto& e : kEntries) {
        if (is_user_field(e.field)) {
            s.add(e.field);
        }
    }
    return s;
}

FieldSet FieldSet::metadata() {
    FieldSet s;
    for (const auto& e : kEntries) {
        if (!is_user_field(e.field)) {
            s.add(e.field);
        }
    }
    return s;
}

const std::vector<Field>& all_fields() {
    static const std::vector<Field> kAll = [] {
        std::vector<Field> v;
        for (const auto& e : kEntries) {
            v.push_back(e.field);
        }
        return v;
    }();
    return kAll;
}

std::string_view token_of(Field f) {
    for (const auto& e : kEntries) {
        if (e.field == f) {
            return e.token;
        }
    }
    return {};
}

std::optional<Field> field_from_token(std::string_view token) {
    const std::string norm = normalize(token);
    for (const auto& e : kEntries) {
        if (e.token == norm) {
            return e.field;
        }
    }
    return std::nullopt;
}

FieldSet parse_field_list(std::string_view csv) {
    FieldSet s;
    std::size_t start = 0;
    while (start <= csv.size()) {
        std::size_t comma = csv.find(',', start);
        if (comma == std::string_view::npos) {
            comma = csv.size();
        }
        const std::string_view part = csv.substr(start, comma - start);
        start = comma + 1;
        const std::string norm = normalize(part);
        if (norm.empty()) {
            continue;
        }
        const auto f = field_from_token(norm);
        if (!f) {
            throw FieldError("unknown field: '" + norm + "'");
        }
        s.add(*f);
    }
    if (s.empty()) {
        throw FieldError("no fields selected");
    }
    return s;
}

std::optional<FieldSet> builtin_preset(std::string_view name) {
    if (name == "all") {
        return FieldSet::all();
    }
    if (name == "userdata") {
        return FieldSet::userdata();
    }
    if (name == "metadata") {
        return FieldSet::metadata();
    }
    return std::nullopt;
}

bool is_builtin_preset(std::string_view name) { return builtin_preset(name).has_value(); }

FieldSet resolve_selection(std::string_view preset, std::string_view fields_csv,
                           const config::Config& config, const FieldSet& fallback) {
    if (!fields_csv.empty()) {
        return parse_field_list(fields_csv);
    }
    if (!preset.empty()) {
        if (auto builtin = builtin_preset(preset)) {
            return *builtin;
        }
        if (auto user = config.preset(std::string(preset))) {
            FieldSet s;
            for (const auto& tok : *user) {
                const auto f = field_from_token(tok);
                if (!f) {
                    throw FieldError("preset '" + std::string(preset) +
                                     "' has an unknown field: '" + tok + "'");
                }
                s.add(*f);
            }
            if (s.empty()) {
                throw FieldError("preset '" + std::string(preset) + "' is empty");
            }
            return s;
        }
        throw FieldError("unknown preset: '" + std::string(preset) + "'");
    }
    return fallback;
}

}  // namespace pfdb::io
