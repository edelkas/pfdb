#include "db/repository.hpp"

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
    for (const char* table :
         {"genres", "credits", "ratings", "source_refs", "video_files"}) {
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
            "created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
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
            "updated_at=? WHERE id=?");
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
        stmt.bind(11, film.id);
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
    return f;
}

constexpr const char* kFilmColumns =
    "id, title, original_title, year, runtime_minutes, synopsis, date_watched, "
    "personal_rating, notes, favorite, created_at, updated_at";

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
        SQLite::Statement q(
            *db_,
            "SELECT p.name, c.role, c.character, c.ord FROM credits c "
            "JOIN people p ON p.id = c.person_id WHERE c.film_id = ? ORDER BY c.ord");
        q.bind(1, id);
        while (q.executeStep()) {
            Credit c;
            c.person.name = q.getColumn(0).getString();
            c.role = credit_role_from_string(q.getColumn(1).getString());
            c.character = q.getColumn(2).getString();
            c.order = q.getColumn(3).getInt();
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
        SQLite::Statement q(
            *db_,
            "SELECT c.film_id, p.name, c.role, c.character, c.ord FROM credits c "
            "JOIN people p ON p.id = c.person_id ORDER BY c.film_id, c.ord");
        while (q.executeStep()) {
            if (Film* f = film_at(q.getColumn(0).getInt64())) {
                Credit c;
                c.person.name = q.getColumn(1).getString();
                c.role = credit_role_from_string(q.getColumn(2).getString());
                c.character = q.getColumn(3).getString();
                c.order = q.getColumn(4).getInt();
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

}  // namespace pfdb::db
