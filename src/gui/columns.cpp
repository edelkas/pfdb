#include "gui/columns.hpp"

#include <array>
#include <cstdio>

#include "query/field.hpp"

namespace pfdb::gui {
namespace {

bool is_rating(std::string_view token) {
    return token == "imdb_rating" || token == "fa_rating" || token == "my_rating";
}

std::string join(const std::vector<std::string>& xs) {
    std::string out;
    for (const auto& x : xs) {
        if (!out.empty()) {
            out += ", ";
        }
        out += x;
    }
    return out;
}

}  // namespace

const std::vector<Column>& all_columns() {
    // {token, header, default_visible, sortable, numeric}
    static const std::vector<Column> kCols = {
        {"year", "Year", true, true, true},
        {"title", "Title", true, true, false},
        {"date_watched", "Last seen", true, true, false},
        {"runtime", "Length", true, true, true},
        {"director", "Director", true, false, false},
        {"genre", "Genres", true, false, false},
        {"fa_rating", "FA", true, true, true},
        {"imdb_rating", "IMDb", true, true, true},
        // Hidden by default; toggled via the table header's context menu.
        {"original_title", "Original title", false, true, false},
        {"spanish_title", "Spanish title", false, true, false},
        {"my_rating", "My rating", false, true, true},
        {"watch_count", "Watches", false, true, true},
        {"review_count", "Reviews", false, true, true},
        {"budget", "Budget", false, true, true},
        {"gross", "Gross", false, true, true},
        {"writer", "Writers", false, false, false},
        {"topic", "Topics", false, false, false},
        {"group", "Groups", false, false, false},
    };
    return kCols;
}

std::string format_field(std::string_view token, const Film& film) {
    const auto info = query::find_field(token);
    if (!info) {
        return {};
    }
    switch (info->kind) {
        case query::FieldKind::Text:
            return query::extract_text(info->name, film).value_or("");
        case query::FieldKind::Date:
            return query::extract_date(info->name, film).value_or("");
        case query::FieldKind::Number: {
            const auto v = query::extract_number(info->name, film);
            if (!v) {
                return {};
            }
            std::array<char, 32> buf{};
            if (is_rating(info->name)) {
                std::snprintf(buf.data(), buf.size(), "%.1f", *v);
            } else {
                std::snprintf(buf.data(), buf.size(), "%lld",
                              static_cast<long long>(*v));
            }
            return std::string(buf.data());
        }
        case query::FieldKind::StringList:
        case query::FieldKind::NameList:
            return join(query::extract_strings(info->name, film));
        case query::FieldKind::IdList:
            return {};
    }
    return {};
}

}  // namespace pfdb::gui
