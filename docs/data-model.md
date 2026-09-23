# Data model & schema

## Domain types

All domain types live in `include/pfdb/` and are plain values with `==`.

- **`Film`** — the central aggregate: core metadata (title, year, runtime,
  synopsis, genres), `credits`, `ratings`, `source_refs`, embedded `UserData`,
  an optional `VideoFileInfo`, and bookkeeping timestamps. FilmAffinity adds
  `spanish_title`, `spanish_synopsis`, `review_count`, and the tag-style lists
  `topics` (finer than genres) and `groups` (sagas). Film-to-film **relations**
  and **similarities** are edges between collection films, stored in their own
  tables rather than on `Film`.
- **`Person`** / **`Credit`** — a person and their contribution to a film
  (`CreditRole` = Director, Writer, Actor, …), with character and billing order.
  A loaded `Credit` carries its person's stored id, which is what the query
  engine's cast/crew inclusion filters match on (see [querying.md](querying.md)).
- **`Rating`** — a score from one source, stored verbatim (`value` on `scale`)
  plus optional `votes`. No lossy normalization at rest.
- **`SourceRef`** — the film's identity on an external source (`source` +
  `external_id`, e.g. `imdb`/`tt0083658`), plus when it was last fetched. A film
  may carry several — this is what lets PFDB combine sources.
- **`UserData`** — the user's own data: date watched, personal rating, notes,
  favourite flag, watch count, and owned / wish-list flags.
- **`VideoFileInfo`** — metadata about a local video file, all optional.

### Conventions

- **Ids** (`pfdb::Id`, `int64`): `0` (`kInvalidId`) means "not yet persisted".
- **Timestamps** (`UnixSeconds`): UTC Unix seconds, one representation for DB,
  JSON, and formatting.
- **Dates** (`IsoDate`): `"YYYY-MM-DD"` strings — exactly what the user types for
  "date watched".

## SQLite schema

The `Repository` migrates the database to the latest version on open. The
current version is stored in SQLite's `PRAGMA user_version`.

```
-- v1
films(id PK, title, original_title, year?, runtime_minutes?, synopsis,
      date_watched?, personal_rating?, notes, favorite, created_at?, updated_at?,
      -- v2 additions:
      spanish_title, spanish_synopsis, review_count?,
      -- v3 additions:
      watch_count, owned, wishlist)

genres(film_id → films, genre, ord)
people(id PK, name UNIQUE)
credits(film_id → films, person_id → people, role, character, ord)
ratings(film_id → films, source, value, scale, votes?)
source_refs(film_id → films, source, external_id, fetched_at?)
video_files(film_id PK → films, path, size_bytes?, duration_seconds?,
            width?, height?, codec)

-- v2
topics(film_id → films, topic, ord)            -- FA temas, like genres
movie_groups(film_id → films, name, ord)       -- FA groups/sagas, like genres
relations(from_id → films, to_id → films, kind, PK(from_id,to_id))    -- directed
similarities(a_id → films, b_id → films, percent, PK(a_id,b_id))      -- a_id<b_id
```

`?` marks nullable columns (they map to `std::optional` fields). All child tables
reference `films(id)` with `ON DELETE CASCADE`, and `PRAGMA foreign_keys` is on,
so removing a film removes everything hanging off it — including any relation or
similarity edge at either endpoint. People are normalized and de-duplicated by
name.

**Edges.** `relations` are directed and carry a Spanish `kind` label from
FilmAffinity ("tiene secuela", …), stored from the source film's perspective.
`similarities` are undirected (stored canonically with `a_id < b_id`) and carry a
percentage. Both are only ever stored between films already in the collection;
see [sources/filmaffinity.md](sources/filmaffinity.md).

## Migrations

Migrations are an ordered list in `src/db/repository.cpp` (`kMigrations`); entry
*i* upgrades the schema from version *i* to *i+1*, and the latest version is
derived from the list length. To evolve the schema, **append** a new entry —
never edit an existing one — so older databases upgrade cleanly. Each migration
runs in a single transaction.
