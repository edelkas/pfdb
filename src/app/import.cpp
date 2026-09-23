#include "app/import.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "db/repository.hpp"
#include "io/emdb/emdb_decode.hpp"
#include "io/emdb/emdb_map.hpp"
#include "io/emdb/emdb_parser.hpp"
#include "pfdb/film.hpp"

namespace pfdb::app {
namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("could not open file '" + path + "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::optional<std::string> imdb_ref(const Film& f) {
    for (const auto& ref : f.source_refs) {
        if (ref.source == "imdb") {
            return ref.external_id;
        }
    }
    return std::nullopt;
}

bool role_selected(CreditRole role, const io::FieldSet& sel) {
    switch (role) {
        case CreditRole::Actor:    return sel.contains(io::Field::Cast);
        case CreditRole::Director: return sel.contains(io::Field::Directors);
        case CreditRole::Writer:   return sel.contains(io::Field::Writers);
        case CreditRole::Composer: return sel.contains(io::Field::Composers);
        default:                   return false;
    }
}

/// Copy the selected fields of `src` onto `dst`, leaving everything else intact.
void apply_selected(Film& dst, const Film& src, const io::FieldSet& sel) {
    using io::Field;
    if (sel.contains(Field::Title)) dst.title = src.title;
    if (sel.contains(Field::OriginalTitle)) dst.original_title = src.original_title;
    if (sel.contains(Field::Year)) dst.year = src.year;
    if (sel.contains(Field::Runtime)) dst.runtime_minutes = src.runtime_minutes;
    if (sel.contains(Field::Synopsis)) dst.synopsis = src.synopsis;
    if (sel.contains(Field::Genres)) dst.genres = src.genres;
    if (sel.contains(Field::Topics)) dst.topics = src.topics;
    if (sel.contains(Field::Groups)) dst.groups = src.groups;

    // Credits: replace only the roles that were selected.
    if (sel.contains(Field::Cast) || sel.contains(Field::Directors) ||
        sel.contains(Field::Writers) || sel.contains(Field::Composers)) {
        std::vector<Credit> kept;
        for (auto& c : dst.credits) {
            if (!role_selected(c.role, sel)) {
                kept.push_back(std::move(c));
            }
        }
        for (const auto& c : src.credits) {
            if (role_selected(c.role, sel)) {
                kept.push_back(c);
            }
        }
        dst.credits = std::move(kept);
    }

    if (sel.contains(Field::ImdbId)) {
        if (auto ext = imdb_ref(src); ext && !imdb_ref(dst)) {
            dst.source_refs.push_back({"imdb", *ext, std::nullopt});
        }
    }
    if (sel.contains(Field::ImdbRating)) {
        std::vector<Rating> kept;
        for (auto& r : dst.ratings) {
            if (r.source != "imdb") {
                kept.push_back(std::move(r));
            }
        }
        for (const auto& r : src.ratings) {
            if (r.source == "imdb") {
                kept.push_back(r);
            }
        }
        dst.ratings = std::move(kept);
    }

    if (sel.contains(Field::UserRating)) dst.user.personal_rating = src.user.personal_rating;
    if (sel.contains(Field::WatchDate)) dst.user.date_watched = src.user.date_watched;
    if (sel.contains(Field::WatchCount)) dst.user.watch_count = src.user.watch_count;
    if (sel.contains(Field::Owned)) dst.user.owned = src.user.owned;
    if (sel.contains(Field::Wishlist)) dst.user.wishlist = src.user.wishlist;
    if (sel.contains(Field::Favorite)) dst.user.favorite = src.user.favorite;
    if (sel.contains(Field::Comments)) dst.user.notes = src.user.notes;
    if (sel.contains(Field::VideoFile)) dst.video = src.video;
}

}  // namespace

ImportStats import_emdb(db::Repository& repo, const std::string& path,
                        const io::FieldSet& selection, bool dry_run) {
    const std::string bytes = read_file(path);
    const std::string json = io::emdb::bytes_to_json(bytes);
    const io::emdb::EmdbData data = io::emdb::parse(json);
    const std::vector<Film> parsed = io::emdb::to_films(data, selection);

    ImportStats stats;
    for (const auto& src : parsed) {
        ++stats.total;
        std::optional<Id> existing_id;
        if (auto ext = imdb_ref(src)) {
            existing_id = repo.find_id_by_source_ref("imdb", *ext);
        }

        if (existing_id) {
            ++stats.updated;
            if (!dry_run) {
                Film existing = *repo.find(*existing_id);
                apply_selected(existing, src, selection);
                repo.update(existing);
            }
        } else {
            ++stats.added;
            if (!dry_run) {
                Film fresh;
                apply_selected(fresh, src, selection);
                repo.insert(fresh);
            }
        }
    }
    return stats;
}

}  // namespace pfdb::app
