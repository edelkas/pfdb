#include "app/enrichment.hpp"

#include <algorithm>
#include <iterator>

namespace pfdb::app {

Film merge_films(const std::optional<Film>& imdb, const std::optional<Film>& fa) {
    // Single-source cases pass straight through.
    if (!fa.has_value()) {
        return imdb.value_or(Film{});
    }
    if (!imdb.has_value()) {
        return *fa;
    }

    // Both present: IMDb is the base; overlay FilmAffinity's specialties.
    Film merged = *imdb;
    merged.spanish_title = fa->spanish_title;
    merged.spanish_synopsis = fa->spanish_synopsis;
    merged.review_count = fa->review_count;
    merged.topics = fa->topics;
    merged.groups = fa->groups;

    // FA's rating(s) join IMDb's; FA's source_refs join IMDb's.
    merged.ratings.insert(merged.ratings.end(), fa->ratings.begin(), fa->ratings.end());
    merged.source_refs.insert(merged.source_refs.end(), fa->source_refs.begin(),
                              fa->source_refs.end());
    return merged;
}

void apply_overrides(Film& film, const FilmOverrides& ov) {
    if (ov.title.has_value()) {
        film.title = *ov.title;
    }
    if (ov.year.has_value()) {
        film.year = ov.year;
    }
    if (ov.favorite) {
        film.user.favorite = true;
    }
    if (ov.date_watched.has_value()) {
        film.user.date_watched = ov.date_watched;
    }
    if (ov.personal_rating.has_value()) {
        film.user.personal_rating = ov.personal_rating;
    }
    if (ov.notes.has_value()) {
        film.user.notes = *ov.notes;
    }
    for (const auto& genre : ov.add_genres) {
        if (std::find(film.genres.begin(), film.genres.end(), genre) == film.genres.end()) {
            film.genres.push_back(genre);
        }
    }
}

}  // namespace pfdb::app
