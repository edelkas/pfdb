#include "db/repository.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

namespace pfdb::db {
namespace {

/// Ordered schema migrations. Index i upgrades the database from version i to
/// version i+1. To evolve the schema, append a new entry here and bump nothing
/// else -- latest_schema_version() is derived from the array size.
///
/// Each entry may contain several statements separated by ';'; they run inside
/// one transaction so a migration is all-or-nothing.
constexpr std::array kMigrations = {
    // v0 -> v1: initial schema.
    R"sql(
        CREATE TABLE films (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            title           TEXT    NOT NULL,
            original_title  TEXT    NOT NULL DEFAULT '',
            year            INTEGER,
            runtime_minutes INTEGER,
            synopsis        TEXT    NOT NULL DEFAULT '',
            date_watched    TEXT,
            personal_rating REAL,
            notes           TEXT    NOT NULL DEFAULT '',
            favorite        INTEGER NOT NULL DEFAULT 0,
            created_at      INTEGER,
            updated_at      INTEGER
        );

        CREATE TABLE genres (
            film_id INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            genre   TEXT    NOT NULL,
            ord     INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX idx_genres_film ON genres(film_id);

        CREATE TABLE people (
            id   INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT    NOT NULL UNIQUE
        );

        CREATE TABLE credits (
            film_id   INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            person_id INTEGER NOT NULL REFERENCES people(id),
            role      TEXT    NOT NULL,
            character TEXT    NOT NULL DEFAULT '',
            ord       INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX idx_credits_film ON credits(film_id);

        CREATE TABLE ratings (
            film_id INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            source  TEXT    NOT NULL,
            value   REAL    NOT NULL,
            scale   REAL    NOT NULL DEFAULT 10,
            votes   INTEGER
        );
        CREATE INDEX idx_ratings_film ON ratings(film_id);

        CREATE TABLE source_refs (
            film_id     INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            source      TEXT    NOT NULL,
            external_id TEXT    NOT NULL,
            fetched_at  INTEGER
        );
        CREATE INDEX idx_source_refs_film ON source_refs(film_id);

        CREATE TABLE video_files (
            film_id          INTEGER PRIMARY KEY REFERENCES films(id) ON DELETE CASCADE,
            path             TEXT    NOT NULL,
            size_bytes       INTEGER,
            duration_seconds INTEGER,
            width            INTEGER,
            height           INTEGER,
            codec            TEXT    NOT NULL DEFAULT ''
        );
    )sql",
    // v1 -> v2: FilmAffinity fields, topics/groups tags, and film-to-film edges.
    R"sql(
        ALTER TABLE films ADD COLUMN spanish_title    TEXT NOT NULL DEFAULT '';
        ALTER TABLE films ADD COLUMN spanish_synopsis TEXT NOT NULL DEFAULT '';
        ALTER TABLE films ADD COLUMN review_count     INTEGER;

        CREATE TABLE topics (
            film_id INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            topic   TEXT    NOT NULL,
            ord     INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX idx_topics_film ON topics(film_id);

        CREATE TABLE movie_groups (
            film_id INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            name    TEXT    NOT NULL,
            ord     INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX idx_movie_groups_film ON movie_groups(film_id);

        CREATE TABLE relations (
            from_id INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            to_id   INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            kind    TEXT    NOT NULL DEFAULT '',
            PRIMARY KEY (from_id, to_id)
        );
        CREATE INDEX idx_relations_to ON relations(to_id);

        CREATE TABLE similarities (
            a_id    INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            b_id    INTEGER NOT NULL REFERENCES films(id) ON DELETE CASCADE,
            percent INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (a_id, b_id)
        );
        CREATE INDEX idx_similarities_b ON similarities(b_id);
    )sql",
    // v2 -> v3: extra user-specific fields (from EMDB import and general use).
    R"sql(
        ALTER TABLE films ADD COLUMN watch_count INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE films ADD COLUMN owned       INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE films ADD COLUMN wishlist    INTEGER NOT NULL DEFAULT 0;
    )sql",
};

UnixSeconds now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// --- Small helpers for reading nullable columns into std::optional. ---

std::optional<int> get_opt_int(SQLite::Statement& stmt, int col) {
    if (stmt.getColumn(col).isNull()) {
        return std::nullopt;
    }
    return stmt.getColumn(col).getInt();
}

std::optional<std::int64_t> get_opt_int64(SQLite::Statement& stmt, int col) {
    if (stmt.getColumn(col).isNull()) {
        return std::nullopt;
    }
    return stmt.getColumn(col).getInt64();
}

std::optional<double> get_opt_double(SQLite::Statement& stmt, int col) {
    if (stmt.getColumn(col).isNull()) {
        return std::nullopt;
    }
    return stmt.getColumn(col).getDouble();
}

std::optional<std::string> get_opt_text(SQLite::Statement& stmt, int col) {
    if (stmt.getColumn(col).isNull()) {
        return std::nullopt;
    }
    return stmt.getColumn(col).getString();
}

// Bind an optional to a parameter, using SQL NULL when unset.
template <typename T>
void bind_opt(SQLite::Statement& stmt, int index, const std::optional<T>& value) {
    if (value.has_value()) {
        stmt.bind(index, *value);
    } else {
        stmt.bind(index);  // NULL
    }
}

Id upsert_person(SQLite::Database& db, const std::string& name) {
    {
        SQLite::Statement sel(db, "SELECT id FROM people WHERE name = ?");
        sel.bind(1, name);
        if (sel.executeStep()) {
            return sel.getColumn(0).getInt64();
        }
    }
    SQLite::Statement ins(db, "INSERT INTO people(name) VALUES(?)");
    ins.bind(1, name);
    ins.exec();
    return db.getLastInsertRowid();
}

}  // namespace

Repository::Repository(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    db_->exec("PRAGMA foreign_keys = ON");
    db_->exec("PRAGMA journal_mode = WAL");
    migrate();
}

Repository::~Repository() = default;
Repository::Repository(Repository&&) noexcept = default;
Repository& Repository::operator=(Repository&&) noexcept = default;

int Repository::latest_schema_version() {
    return static_cast<int>(kMigrations.size());
}

int Repository::schema_version() const {
    return db_->execAndGet("PRAGMA user_version").getInt();
}

void Repository::migrate() {
    int current = schema_version();
    const int target = latest_schema_version();
    if (current > target) {
        throw std::runtime_error(
            "database schema is newer than this build supports; please upgrade PFDB");
    }
    while (current < target) {
        SQLite::Transaction txn(*db_);
        db_->exec(kMigrations[static_cast<std::size_t>(current)]);
        ++current;
        // user_version can't be parameterized; current is a trusted int.
        db_->exec("PRAGMA user_version = " + std::to_string(current));
        txn.commit();
    }
}

void Repository::write_related(Id film_id, const Film& film) {
    {
        SQLite::Statement stmt(
            *db_, "INSERT INTO genres(film_id, genre, ord) VALUES(?, ?, ?)");
        int ord = 0;
        for (const auto& genre : film.genres) {
            stmt.bind(1, film_id);
            stmt.bind(2, genre);
            stmt.bind(3, ord++);
            stmt.exec();
            stmt.reset();
        }
    }
    {
        SQLite::Statement stmt(
            *db_, "INSERT INTO topics(film_id, topic, ord) VALUES(?, ?, ?)");
        int ord = 0;
        for (const auto& topic : film.topics) {
            stmt.bind(1, film_id);
            stmt.bind(2, topic);
            stmt.bind(3, ord++);
            stmt.exec();
            stmt.reset();
        }
    }
    {
        SQLite::Statement stmt(
            *db_, "INSERT INTO movie_groups(film_id, name, ord) VALUES(?, ?, ?)");
        int ord = 0;
        for (const auto& group : film.groups) {
            stmt.bind(1, film_id);
            stmt.bind(2, group);
            stmt.bind(3, ord++);
            stmt.exec();
            stmt.reset();
        }
    }
    {
        SQLite::Statement stmt(
            *db_,
            "INSERT INTO credits(film_id, person_id, role, character, ord) "
            "VALUES(?, ?, ?, ?, ?)");
        for (const auto& credit : film.credits) {
            const Id person_id = upsert_person(*db_, credit.person.name);
            stmt.bind(1, film_id);
            stmt.bind(2, person_id);
            stmt.bind(3, std::string(to_string(credit.role)));
            stmt.bind(4, credit.character);
            stmt.bind(5, credit.order);
            stmt.exec();
            stmt.reset();
        }
    }
    {
        SQLite::Statement stmt(
            *db_,
            "INSERT INTO ratings(film_id, source, value, scale, votes) "
            "VALUES(?, ?, ?, ?, ?)");
        for (const auto& rating : film.ratings) {
            stmt.bind(1, film_id);
            stmt.bind(2, rating.source);
            stmt.bind(3, rating.value);
            stmt.bind(4, rating.scale);
            bind_opt(stmt, 5, rating.votes);
            stmt.exec();
            stmt.reset();
        }
    }
    {
        SQLite::Statement stmt(
            *db_,
            "INSERT INTO source_refs(film_id, source, external_id, fetched_at) "
            "VALUES(?, ?, ?, ?)");
        for (const auto& ref : film.source_refs) {
            stmt.bind(1, film_id);
            stmt.bind(2, ref.source);
            stmt.bind(3, ref.external_id);
            bind_opt(stmt, 4, ref.fetched_at);
            stmt.exec();
            stmt.reset();
        }
    }
    if (film.video.has_value()) {
        const auto& v = *film.video;
        SQLite::Statement stmt(
            *db_,
            "INSERT INTO video_files(film_id, path, size_bytes, duration_seconds, "
            "width, height, codec) VALUES(?, ?, ?, ?, ?, ?, ?)");
        stmt.bind(1, film_id);
        stmt.bind(2, v.path);
        bind_opt(stmt, 3, v.size_bytes);
        bind_opt(stmt, 4, v.duration_seconds);
        bind_opt(stmt, 5, v.width);
        bind_opt(stmt, 6, v.height);
        stmt.bind(7, v.codec);
        stmt.exec();
    }
}

void Repository::delete_related(Id film_id) {
    for (const char* table : {"genres", "topics", "movie_groups", "credits", "ratings",
                              "source_refs", "video_files"}) {
        SQLite::Statement stmt(*db_,
                               std::string("DELETE FROM ") + table + " WHERE film_id = ?");
        stmt.bind(1, film_id);
        stmt.exec();
    }
}

Id Repository::insert(const Film& film) {
    SQLite::Transaction txn(*db_);
    const UnixSeconds ts = now_unix();
    {
        SQLite::Statement stmt(
            *db_,
            "INSERT INTO films(title, original_title, year, runtime_minutes, "
            "synopsis, date_watched, personal_rating, notes, favorite, "
            "created_at, updated_at, spanish_title, spanish_synopsis, review_count, "
            "watch_count, owned, wishlist) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        stmt.bind(1, film.title);
        stmt.bind(2, film.original_title);
        bind_opt(stmt, 3, film.year);
        bind_opt(stmt, 4, film.runtime_minutes);
        stmt.bind(5, film.synopsis);
        bind_opt(stmt, 6, film.user.date_watched);
        bind_opt(stmt, 7, film.user.personal_rating);
        stmt.bind(8, film.user.notes);
        stmt.bind(9, film.user.favorite ? 1 : 0);
        stmt.bind(10, ts);
        stmt.bind(11, ts);
        stmt.bind(12, film.spanish_title);
        stmt.bind(13, film.spanish_synopsis);
        bind_opt(stmt, 14, film.review_count);
        stmt.bind(15, film.user.watch_count);
        stmt.bind(16, film.user.owned ? 1 : 0);
        stmt.bind(17, film.user.wishlist ? 1 : 0);
        stmt.exec();
    }
    const Id film_id = db_->getLastInsertRowid();
    write_related(film_id, film);
    txn.commit();
    return film_id;
}

bool Repository::update(const Film& film) {
    SQLite::Transaction txn(*db_);
    const UnixSeconds ts = now_unix();
    int changed = 0;
    {
        SQLite::Statement stmt(
            *db_,
            "UPDATE films SET title=?, original_title=?, year=?, runtime_minutes=?, "
            "synopsis=?, date_watched=?, personal_rating=?, notes=?, favorite=?, "
            "updated_at=?, spanish_title=?, spanish_synopsis=?, review_count=?, "
            "watch_count=?, owned=?, wishlist=? "
            "WHERE id=?");
        stmt.bind(1, film.title);
        stmt.bind(2, film.original_title);
        bind_opt(stmt, 3, film.year);
        bind_opt(stmt, 4, film.runtime_minutes);
        stmt.bind(5, film.synopsis);
        bind_opt(stmt, 6, film.user.date_watched);
        bind_opt(stmt, 7, film.user.personal_rating);
        stmt.bind(8, film.user.notes);
        stmt.bind(9, film.user.favorite ? 1 : 0);
        stmt.bind(10, ts);
        stmt.bind(11, film.spanish_title);
        stmt.bind(12, film.spanish_synopsis);
        bind_opt(stmt, 13, film.review_count);
        stmt.bind(14, film.user.watch_count);
        stmt.bind(15, film.user.owned ? 1 : 0);
        stmt.bind(16, film.user.wishlist ? 1 : 0);
        stmt.bind(17, film.id);
        changed = stmt.exec();
    }
    if (changed == 0) {
        return false;  // no such film; transaction rolls back on scope exit
    }
    delete_related(film.id);
    write_related(film.id, film);
    txn.commit();
    return true;
}

bool Repository::remove(Id id) {
    SQLite::Statement stmt(*db_, "DELETE FROM films WHERE id = ?");
    stmt.bind(1, id);
    return stmt.exec() > 0;
}

std::int64_t Repository::count() const {
    return db_->execAndGet("SELECT COUNT(*) FROM films").getInt64();
}

namespace {

// Read the scalar columns of the `films` row the statement is positioned on.
Film read_film_row(SQLite::Statement& stmt) {
    Film f;
    f.id = stmt.getColumn(0).getInt64();
    f.title = stmt.getColumn(1).getString();
    f.original_title = stmt.getColumn(2).getString();
    f.year = get_opt_int(stmt, 3);
    f.runtime_minutes = get_opt_int(stmt, 4);
    f.synopsis = stmt.getColumn(5).getString();
    f.user.date_watched = get_opt_text(stmt, 6);
    f.user.personal_rating = get_opt_double(stmt, 7);
    f.user.notes = stmt.getColumn(8).getString();
    f.user.favorite = stmt.getColumn(9).getInt() != 0;
    f.created_at = get_opt_int64(stmt, 10);
    f.updated_at = get_opt_int64(stmt, 11);
    f.spanish_title = stmt.getColumn(12).getString();
    f.spanish_synopsis = stmt.getColumn(13).getString();
    f.review_count = get_opt_int(stmt, 14);
    f.user.watch_count = stmt.getColumn(15).getInt();
    f.user.owned = stmt.getColumn(16).getInt() != 0;
    f.user.wishlist = stmt.getColumn(17).getInt() != 0;
    return f;
}

constexpr const char* kFilmColumns =
    "id, title, original_title, year, runtime_minutes, synopsis, date_watched, "
    "personal_rating, notes, favorite, created_at, updated_at, "
    "spanish_title, spanish_synopsis, review_count, watch_count, owned, wishlist";

}  // namespace

std::optional<Film> Repository::find(Id id) const {
    SQLite::Statement stmt(
        *db_, std::string("SELECT ") + kFilmColumns + " FROM films WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    Film f = read_film_row(stmt);

    {
        SQLite::Statement q(*db_,
                            "SELECT genre FROM genres WHERE film_id = ? ORDER BY ord");
        q.bind(1, id);
        while (q.executeStep()) {
            f.genres.emplace_back(q.getColumn(0).getString());
        }
    }
    {
        SQLite::Statement q(*db_,
                            "SELECT topic FROM topics WHERE film_id = ? ORDER BY ord");
        q.bind(1, id);
        while (q.executeStep()) {
            f.topics.emplace_back(q.getColumn(0).getString());
        }
    }
    {
        SQLite::Statement q(
            *db_, "SELECT name FROM movie_groups WHERE film_id = ? ORDER BY ord");
        q.bind(1, id);
        while (q.executeStep()) {
            f.groups.emplace_back(q.getColumn(0).getString());
        }
    }
    {
        SQLite::Statement q(
            *db_,
            "SELECT p.id, p.name, c.role, c.character, c.ord FROM credits c "
            "JOIN people p ON p.id = c.person_id WHERE c.film_id = ? ORDER BY c.ord");
        q.bind(1, id);
        while (q.executeStep()) {
            Credit c;
            c.person.id = q.getColumn(0).getInt64();
            c.person.name = q.getColumn(1).getString();
            c.role = credit_role_from_string(q.getColumn(2).getString());
            c.character = q.getColumn(3).getString();
            c.order = q.getColumn(4).getInt();
            f.credits.push_back(std::move(c));
        }
    }
    {
        SQLite::Statement q(
            *db_, "SELECT source, value, scale, votes FROM ratings WHERE film_id = ?");
        q.bind(1, id);
        while (q.executeStep()) {
            Rating r;
            r.source = q.getColumn(0).getString();
            r.value = q.getColumn(1).getDouble();
            r.scale = q.getColumn(2).getDouble();
            r.votes = get_opt_int64(q, 3);
            f.ratings.push_back(std::move(r));
        }
    }
    {
        SQLite::Statement q(
            *db_,
            "SELECT source, external_id, fetched_at FROM source_refs WHERE film_id = ?");
        q.bind(1, id);
        while (q.executeStep()) {
            SourceRef ref;
            ref.source = q.getColumn(0).getString();
            ref.external_id = q.getColumn(1).getString();
            ref.fetched_at = get_opt_int64(q, 2);
            f.source_refs.push_back(std::move(ref));
        }
    }
    {
        SQLite::Statement q(
            *db_,
            "SELECT path, size_bytes, duration_seconds, width, height, codec "
            "FROM video_files WHERE film_id = ?");
        q.bind(1, id);
        if (q.executeStep()) {
            VideoFileInfo v;
            v.path = q.getColumn(0).getString();
            v.size_bytes = get_opt_int64(q, 1);
            v.duration_seconds = get_opt_int(q, 2);
            v.width = get_opt_int(q, 3);
            v.height = get_opt_int(q, 4);
            v.codec = q.getColumn(5).getString();
            f.video = std::move(v);
        }
    }
    return f;
}

std::vector<Film> Repository::load_all() const {
    std::vector<Film> films;
    std::unordered_map<Id, std::size_t> index;  // film id -> position in `films`

    {
        SQLite::Statement stmt(
            *db_, std::string("SELECT ") + kFilmColumns + " FROM films ORDER BY id");
        while (stmt.executeStep()) {
            Film f = read_film_row(stmt);
            index[f.id] = films.size();
            films.push_back(std::move(f));
        }
    }
    if (films.empty()) {
        return films;
    }

    auto film_at = [&](Id film_id) -> Film* {
        auto it = index.find(film_id);
        return it == index.end() ? nullptr : &films[it->second];
    };

    {
        SQLite::Statement q(*db_,
                            "SELECT film_id, genre FROM genres ORDER BY film_id, ord");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                f->genres.emplace_back(q.getColumn(1).getString());
            }
        }
    }
    {
        SQLite::Statement q(*db_,
                            "SELECT film_id, topic FROM topics ORDER BY film_id, ord");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                f->topics.emplace_back(q.getColumn(1).getString());
            }
        }
    }
    {
        SQLite::Statement q(
            *db_, "SELECT film_id, name FROM movie_groups ORDER BY film_id, ord");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                f->groups.emplace_back(q.getColumn(1).getString());
            }
        }
    }
    {
        SQLite::Statement q(
            *db_,
            "SELECT c.film_id, p.id, p.name, c.role, c.character, c.ord FROM credits c "
            "JOIN people p ON p.id = c.person_id ORDER BY c.film_id, c.ord");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                Credit c;
                c.person.id = q.getColumn(1).getInt64();
                c.person.name = q.getColumn(2).getString();
                c.role = credit_role_from_string(q.getColumn(3).getString());
                c.character = q.getColumn(4).getString();
                c.order = q.getColumn(5).getInt();
                f->credits.push_back(std::move(c));
            }
        }
    }
    {
        SQLite::Statement q(
            *db_, "SELECT film_id, source, value, scale, votes FROM ratings");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                Rating r;
                r.source = q.getColumn(1).getString();
                r.value = q.getColumn(2).getDouble();
                r.scale = q.getColumn(3).getDouble();
                r.votes = get_opt_int64(q, 4);
                f->ratings.push_back(std::move(r));
            }
        }
    }
    {
        SQLite::Statement q(
            *db_, "SELECT film_id, source, external_id, fetched_at FROM source_refs");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                SourceRef ref;
                ref.source = q.getColumn(1).getString();
                ref.external_id = q.getColumn(2).getString();
                ref.fetched_at = get_opt_int64(q, 3);
                f->source_refs.push_back(std::move(ref));
            }
        }
    }
    {
        SQLite::Statement q(
            *db_,
            "SELECT film_id, path, size_bytes, duration_seconds, width, height, codec "
            "FROM video_files");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                VideoFileInfo v;
                v.path = q.getColumn(1).getString();
                v.size_bytes = get_opt_int64(q, 2);
                v.duration_seconds = get_opt_int(q, 3);
                v.width = get_opt_int(q, 4);
                v.height = get_opt_int(q, 5);
                v.codec = q.getColumn(6).getString();
                f->video = std::move(v);
            }
        }
    }
    return films;
}

std::optional<Id> Repository::find_id_by_source_ref(const std::string& source,
                                                    const std::string& external_id) const {
    SQLite::Statement q(
        *db_, "SELECT film_id FROM source_refs WHERE source = ? AND external_id = ? "
              "LIMIT 1");
    q.bind(1, source);
    q.bind(2, external_id);
    if (q.executeStep()) {
        return q.getColumn(0).getInt64();
    }
    return std::nullopt;
}

void Repository::replace_relations(Id film_id, const std::vector<RelationEdge>& edges) {
    SQLite::Transaction txn(*db_);
    {
        SQLite::Statement del(*db_, "DELETE FROM relations WHERE from_id = ?");
        del.bind(1, film_id);
        del.exec();
    }
    SQLite::Statement ins(
        *db_, "INSERT OR REPLACE INTO relations(from_id, to_id, kind) VALUES(?, ?, ?)");
    for (const auto& e : edges) {
        if (e.other_id == film_id) {
            continue;
        }
        ins.bind(1, film_id);
        ins.bind(2, e.other_id);
        ins.bind(3, e.kind);
        ins.exec();
        ins.reset();
    }
    txn.commit();
}

void Repository::replace_similarities(Id film_id,
                                      const std::vector<SimilarityEdge>& edges) {
    SQLite::Transaction txn(*db_);
    {
        SQLite::Statement del(*db_,
                              "DELETE FROM similarities WHERE a_id = ? OR b_id = ?");
        del.bind(1, film_id);
        del.bind(2, film_id);
        del.exec();
    }
    SQLite::Statement ins(
        *db_,
        "INSERT OR REPLACE INTO similarities(a_id, b_id, percent) VALUES(?, ?, ?)");
    for (const auto& e : edges) {
        if (e.other_id == film_id) {
            continue;
        }
        const Id a = std::min(film_id, e.other_id);
        const Id b = std::max(film_id, e.other_id);
        ins.bind(1, a);
        ins.bind(2, b);
        ins.bind(3, e.percent);
        ins.exec();
        ins.reset();
    }
    txn.commit();
}

std::vector<Repository::RelationEdge> Repository::relations_of(Id film_id) const {
    std::vector<RelationEdge> out;
    SQLite::Statement q(
        *db_, "SELECT to_id, kind FROM relations WHERE from_id = ? ORDER BY to_id");
    q.bind(1, film_id);
    while (q.executeStep()) {
        out.push_back(RelationEdge{q.getColumn(0).getInt64(), q.getColumn(1).getString()});
    }
    return out;
}

std::vector<Repository::SimilarityEdge> Repository::similarities_of(Id film_id) const {
    std::vector<SimilarityEdge> out;
    SQLite::Statement q(
        *db_,
        "SELECT CASE WHEN a_id = ? THEN b_id ELSE a_id END AS other, percent "
        "FROM similarities WHERE a_id = ? OR b_id = ? ORDER BY percent DESC");
    q.bind(1, film_id);
    q.bind(2, film_id);
    q.bind(3, film_id);
    while (q.executeStep()) {
        out.push_back(SimilarityEdge{q.getColumn(0).getInt64(), q.getColumn(1).getInt()});
    }
    return out;
}

std::vector<Repository::RelationPair> Repository::load_relation_pairs() const {
    std::vector<RelationPair> out;
    SQLite::Statement q(*db_, "SELECT from_id, to_id FROM relations");
    while (q.executeStep()) {
        out.push_back(RelationPair{q.getColumn(0).getInt64(), q.getColumn(1).getInt64()});
    }
    return out;
}

std::vector<Repository::SimilarityPair> Repository::load_similarity_pairs() const {
    std::vector<SimilarityPair> out;
    SQLite::Statement q(*db_, "SELECT a_id, b_id FROM similarities");
    while (q.executeStep()) {
        out.push_back(SimilarityPair{q.getColumn(0).getInt64(), q.getColumn(1).getInt64()});
    }
    return out;
}

}  // namespace pfdb::db
