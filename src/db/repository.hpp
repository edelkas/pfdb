#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pfdb/film.hpp"
#include "pfdb/types.hpp"

namespace SQLite {
class Database;
}

namespace pfdb::db {

/// Owns the SQLite connection and maps between the on-disk schema and the
/// domain `Film` aggregate. Opening a Repository creates the database file if
/// needed and brings its schema up to the current version via migrations.
///
/// The Repository is the *only* component that speaks SQL. Higher layers
/// (model, CLI) work exclusively with `Film` values.
class Repository {
public:
    /// Open (creating if absent) the database at `db_path` and migrate it to
    /// the latest schema version. Use ":memory:" for an ephemeral database.
    /// Throws std::runtime_error on failure.
    explicit Repository(const std::string& db_path);
    ~Repository();

    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    Repository(Repository&&) noexcept;
    Repository& operator=(Repository&&) noexcept;

    /// The schema version currently applied to the open database.
    int schema_version() const;

    /// The latest schema version this build knows how to produce.
    static int latest_schema_version();

    /// Insert `film` (its id is ignored) and return the assigned id. Sets
    /// created_at/updated_at on the stored row. Runs in a single transaction.
    Id insert(const Film& film);

    /// Replace the stored film with the same id as `film`. Returns false if
    /// no such film exists. Refreshes updated_at.
    bool update(const Film& film);

    /// Delete the film with `id` (and all its related rows). Returns whether a
    /// row was removed.
    bool remove(Id id);

    /// Fetch a single film by id, or nullopt if absent.
    std::optional<Film> find(Id id) const;

    /// Load every film, fully populated, ordered by id. This is what the
    /// in-memory model calls on startup.
    std::vector<Film> load_all() const;

    /// Number of films stored.
    std::int64_t count() const;

private:
    std::unique_ptr<SQLite::Database> db_;

    void migrate();
    void write_related(Id film_id, const Film& film);
    void delete_related(Id film_id);
};

}  // namespace pfdb::db
