#include "query/field.hpp"

#include <algorithm>
#include <array>

#include "query/text_util.hpp"

namespace pfdb::query {
namespace {

const std::vector<FieldInfo>& registry() {
    static const std::vector<FieldInfo> kFields = {
        // Text
        {"title", FieldKind::Text},
        {"original_title", FieldKind::Text},
        {"spanish_title", FieldKind::Text},
        {"synopsis", FieldKind::Text},
        {"spanish_synopsis", FieldKind::Text},
        {"notes", FieldKind::Text},
        // Number
        {"year", FieldKind::Number},
        {"runtime", FieldKind::Number},
        {"my_rating", FieldKind::Number},
        {"watch_count", FieldKind::Number},
        {"review_count", FieldKind::Number},
        {"imdb_rating", FieldKind::Number},
        {"fa_rating", FieldKind::Number},
        {"budget", FieldKind::Number},
        {"gross", FieldKind::Number},
        // Date
        {"date_watched", FieldKind::Date},
        // String lists (exact membership)
        {"genre", FieldKind::StringList},
        {"topic", FieldKind::StringList},
        {"group", FieldKind::StringList},
        // Name lists (fuzzy name -> id inclusion)
        {"cast", FieldKind::NameList},
        {"director", FieldKind::NameList},
        {"writer", FieldKind::NameList},
        // Id lists (exact id inclusion)
        {"cast_id", FieldKind::IdList},
        {"director_id", FieldKind::IdList},
        {"writer_id", FieldKind::IdList},
        {"related_to", FieldKind::IdList},
        {"similar_to", FieldKind::IdList},
    };
    return kFields;
}

/// Normalize a user token: lowercase, hyphens -> underscores.
std::string normalize_token(std::string_view token) {
    std::string s = to_lower_ascii(token);
    std::replace(s.begin(), s.end(), '-', '_');
    return s;
}

/// Collect the names of credits with `role`.
std::vector<std::string> credit_names(const Film& film, CreditRole role) {
    std::vector<std::string> out;
    for (const auto& c : film.credits) {
        if (c.role == role) {
            out.push_back(c.person.name);
        }
    }
    return out;
}

/// Collect the person ids of credits with `role`.
std::vector<Id> credit_ids(const Film& film, CreditRole role) {
    std::vector<Id> out;
    for (const auto& c : film.credits) {
        if (c.role == role) {
            out.push_back(c.person.id);
        }
    }
    return out;
}

std::vector<Id> edge_ids(const std::unordered_map<Id, std::vector<Id>>* index, Id id) {
    if (index == nullptr) {
        return {};
    }
    auto it = index->find(id);
    return it == index->end() ? std::vector<Id>{} : it->second;
}

}  // namespace

std::optional<FieldInfo> find_field(std::string_view token) {
    const std::string norm = normalize_token(token);
    for (const auto& f : registry()) {
        if (f.name == norm) {
            return f;
        }
    }
    // Tolerate a trailing plural 's' (genres -> genre, topics -> topic, ...).
    if (!norm.empty() && norm.back() == 's') {
        const std::string singular = norm.substr(0, norm.size() - 1);
        for (const auto& f : registry()) {
            if (f.name == singular) {
                return f;
            }
        }
    }
    return std::nullopt;
}

const std::vector<FieldInfo>& all_fields() { return registry(); }

std::string id_field_for(std::string_view name_field) {
    if (name_field == "cast" || name_field == "director" || name_field == "writer") {
        return std::string(name_field) + "_id";
    }
    return {};
}

CreditRole role_for_name_field(std::string_view name_field) {
    if (name_field == "director") {
        return CreditRole::Director;
    }
    if (name_field == "writer") {
        return CreditRole::Writer;
    }
    return CreditRole::Actor;  // "cast"
}

std::optional<double> source_rating(const Film& film, std::string_view source) {
    for (const auto& r : film.ratings) {
        if (r.source == source) {
            return r.value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_text(std::string_view field, const Film& film) {
    if (field == "title") return film.title;
    if (field == "original_title") return film.original_title;
    if (field == "spanish_title") return film.spanish_title;
    if (field == "synopsis") return film.synopsis;
    if (field == "spanish_synopsis") return film.spanish_synopsis;
    if (field == "notes") return film.user.notes;
    return std::nullopt;
}

std::optional<double> extract_number(std::string_view field, const Film& film) {
    if (field == "year") {
        return film.year.has_value() ? std::optional<double>(*film.year) : std::nullopt;
    }
    if (field == "runtime") {
        return film.runtime_minutes.has_value()
                   ? std::optional<double>(*film.runtime_minutes)
                   : std::nullopt;
    }
    if (field == "my_rating") return film.user.personal_rating;
    if (field == "watch_count") return static_cast<double>(film.user.watch_count);
    if (field == "review_count") {
        return film.review_count.has_value() ? std::optional<double>(*film.review_count)
                                             : std::nullopt;
    }
    if (field == "imdb_rating") return source_rating(film, "imdb");
    if (field == "fa_rating") return source_rating(film, "filmaffinity");
    if (field == "budget") {
        return film.budget.has_value() ? std::optional<double>(static_cast<double>(*film.budget))
                                       : std::nullopt;
    }
    if (field == "gross") {
        return film.gross.has_value() ? std::optional<double>(static_cast<double>(*film.gross))
                                      : std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> extract_date(std::string_view field, const Film& film) {
    if (field == "date_watched") return film.user.date_watched;
    return std::nullopt;
}

std::vector<std::string> extract_strings(std::string_view field, const Film& film) {
    if (field == "genre") return film.genres;
    if (field == "topic") return film.topics;
    if (field == "group") return film.groups;
    if (field == "cast") return credit_names(film, CreditRole::Actor);
    if (field == "director") return credit_names(film, CreditRole::Director);
    if (field == "writer") return credit_names(film, CreditRole::Writer);
    return {};
}

std::vector<Id> extract_ids(std::string_view field, const Film& film,
                            const EvalContext& ctx) {
    if (field == "cast_id") return credit_ids(film, CreditRole::Actor);
    if (field == "director_id") return credit_ids(film, CreditRole::Director);
    if (field == "writer_id") return credit_ids(film, CreditRole::Writer);
    if (field == "related_to") return edge_ids(ctx.related, film.id);
    if (field == "similar_to") return edge_ids(ctx.similar, film.id);
    return {};
}

}  // namespace pfdb::query
