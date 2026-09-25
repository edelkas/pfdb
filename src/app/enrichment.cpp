#include "app/enrichment.hpp"

#include <algorithm>
#include <iterator>

namespace pfdb::app {

Film merge_films(const std::optional<Film>& imdb, const std::optional<Film>& fa,
                 const std::optional<Film>& bom) {
    // Base: IMDb if present, else FilmAffinity, else empty.
    Film merged;
    if (imdb.has_value()) {
        merged = *imdb;
        if (fa.has_value()) {
            // Overlay FilmAffinity's specialties (IMDb wins the shared fields).
            merged.spanish_title = fa->spanish_title;
            merged.spanish_synopsis = fa->spanish_synopsis;
            merged.review_count = fa->review_count;
            merged.topics = fa->topics;
            merged.groups = fa->groups;
            merged.ratings.insert(merged.ratings.end(), fa->ratings.begin(),
                                  fa->ratings.end());
            merged.source_refs.insert(merged.source_refs.end(), fa->source_refs.begin(),
                                      fa->source_refs.end());
        }
    } else if (fa.has_value()) {
        merged = *fa;
    }

    // BoxOfficeMojo contributes only financials, plus its source_ref.
    if (bom.has_value()) {
        merged.budget = bom->budget;
        merged.gross = bom->gross;
        merged.source_refs.insert(merged.source_refs.end(), bom->source_refs.begin(),
                                  bom->source_refs.end());
    }
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
