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

    /// Find the internal id of the film carrying the given external source id
    /// (e.g. source="filmaffinity", external_id="358476"), or nullopt.
    std::optional<Id> find_id_by_source_ref(const std::string& source,
                                            const std::string& external_id) const;

    /// A relation edge from a film to another film in the collection.
    struct RelationEdge {
        Id other_id = kInvalidId;
        std::string kind;
    };
    /// A similarity edge with the source's percentage.
    struct SimilarityEdge {
        Id other_id = kInvalidId;
        int percent = 0;
    };

    /// Replace all relation edges *originating from* `film_id` with `edges`
    /// (directed: `film_id` -> other, labelled `kind`).
    void replace_relations(Id film_id, const std::vector<RelationEdge>& edges);

    /// Replace all similarity edges touching `film_id` with `edges` (undirected).
    void replace_similarities(Id film_id, const std::vector<SimilarityEdge>& edges);

    /// Relations where `film_id` is the source (directed out-edges).
    std::vector<RelationEdge> relations_of(Id film_id) const;

    /// Similarities touching `film_id` (either endpoint).
    std::vector<SimilarityEdge> similarities_of(Id film_id) const;

    /// A stored relation edge (directed from_id -> to_id).
    struct RelationPair {
        Id from_id = kInvalidId;
        Id to_id = kInvalidId;
    };
    /// A stored similarity edge (undirected, a_id < b_id).
    struct SimilarityPair {
        Id a_id = kInvalidId;
        Id b_id = kInvalidId;
    };

    /// All relation edges in the database, for bulk-loading the in-memory model.
    std::vector<RelationPair> load_relation_pairs() const;
    /// All similarity edges in the database.
    std::vector<SimilarityPair> load_similarity_pairs() const;

private:
    std::unique_ptr<SQLite::Database> db_;

    void migrate();
    void write_related(Id film_id, const Film& film);
    void delete_related(Id film_id);
};

}  // namespace pfdb::db
