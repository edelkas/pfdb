#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

#include "pfdb/film.hpp"
#include "pfdb/types.hpp"

namespace pfdb::db {
class Repository;
}

namespace pfdb {

/// The whole collection held in memory for fast querying.
///
/// PFDB loads every film on startup (see Repository::load_all) so that
/// filtering, sorting, and searching -- the power-user operations -- run
/// against RAM rather than the database. This M1 version exposes a minimal
/// query surface; the rich filter/regex/sort engine layers on top of it in a
/// later milestone without changing this seam.
///
/// The model is an in-memory view. Callers that mutate the collection are
/// responsible for persisting through the Repository and mirroring the change
/// here (upsert/erase) so the two stay consistent.
class CollectionModel {
public:
    using Predicate = std::function<bool(const Film&)>;

    CollectionModel() = default;
    explicit CollectionModel(std::vector<Film> films);

    /// Build a model by loading every film from the repository.
    static CollectionModel load(const db::Repository& repo);

    /// All films, in load order (by id).
    const std::vector<Film>& all() const noexcept { return films_; }

    std::size_t size() const noexcept { return films_.size(); }
    bool empty() const noexcept { return films_.empty(); }

    /// Pointer to the film with `id`, or nullptr if not present. The pointer is
    /// invalidated by any subsequent upsert/erase.
    const Film* find(Id id) const noexcept;

    /// Every film for which `pred` returns true, in load order.
    std::vector<const Film*> filter(const Predicate& pred) const;

    /// Insert `film`, or replace the existing film with the same id.
    void upsert(Film film);

    /// Remove the film with `id`. Returns whether one was removed.
    bool erase(Id id);

private:
    std::vector<Film> films_;
    std::unordered_map<Id, std::size_t> index_;  // id -> position in films_

    void rebuild_index();
};

}  // namespace pfdb
