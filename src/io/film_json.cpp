#include "io/film_json.hpp"

#include <nlohmann/json.hpp>

namespace pfdb {
namespace {

// nlohmann::json renders a std::optional<T> as either the value or null.
template <typename T>
nlohmann::json opt(const std::optional<T>& value) {
    if (value.has_value()) {
        return *value;
    }
    return nullptr;
}

}  // namespace

nlohmann::json to_json(const Film& film) {
    nlohmann::json j;
    j["id"] = film.id;
    j["title"] = film.title;
    j["original_title"] = film.original_title;
    j["year"] = opt(film.year);
    j["runtime_minutes"] = opt(film.runtime_minutes);
    j["synopsis"] = film.synopsis;
    j["genres"] = film.genres;
    j["spanish_title"] = film.spanish_title;
    j["spanish_synopsis"] = film.spanish_synopsis;
    j["review_count"] = opt(film.review_count);
    j["topics"] = film.topics;
    j["groups"] = film.groups;

    j["credits"] = nlohmann::json::array();
    for (const auto& c : film.credits) {
        j["credits"].push_back({
            {"name", c.person.name},
            {"role", to_string(c.role)},
            {"character", c.character},
            {"order", c.order},
        });
    }

    j["ratings"] = nlohmann::json::array();
    for (const auto& r : film.ratings) {
        j["ratings"].push_back({
            {"source", r.source},
            {"value", r.value},
            {"scale", r.scale},
            {"votes", opt(r.votes)},
        });
    }

    j["source_refs"] = nlohmann::json::array();
    for (const auto& s : film.source_refs) {
        j["source_refs"].push_back({
            {"source", s.source},
            {"external_id", s.external_id},
            {"fetched_at", opt(s.fetched_at)},
        });
    }

    j["user"] = {
        {"date_watched", opt(film.user.date_watched)},
        {"personal_rating", opt(film.user.personal_rating)},
        {"notes", film.user.notes},
        {"favorite", film.user.favorite},
    };

    if (film.video.has_value()) {
        const auto& v = *film.video;
        j["video"] = {
            {"path", v.path},
            {"size_bytes", opt(v.size_bytes)},
            {"duration_seconds", opt(v.duration_seconds)},
            {"width", opt(v.width)},
            {"height", opt(v.height)},
            {"codec", v.codec},
        };
    } else {
        j["video"] = nullptr;
    }

    j["created_at"] = opt(film.created_at);
    j["updated_at"] = opt(film.updated_at);
    return j;
}

}  // namespace pfdb
