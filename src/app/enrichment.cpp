#include "app/enrichment.hpp"

#include <algorithm>

namespace pfdb::app {

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
