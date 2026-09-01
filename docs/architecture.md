# Architecture

PFDB is layered. Each layer depends only on the ones beneath it, and the core
never depends on any specific website.

```
+-----------------------------------------------------------+
|  Frontends:  CLI (now)          →  ImGui GUI (later)       |
+-----------------------------------------------------------+
|  Application / Services                                    |
|   - Collection (in-memory model + query engine)           |
|   - Enrichment (orchestrates source fetch + merge)        |
|   - Import/Export (emdb, CSV/XLS)                          |
|   - Update (GitHub Releases self-update)         [later]   |
+-----------------------------------------------------------+
|  Domain model:  Film, Person, Credit, Rating, SourceRef,  |
|                 UserData, VideoFileInfo                    |
+-----------------------------------------------------------+
|  Sources (plugin registry, ISource interface)   [later]   |
|   IMDbSource | FilmAffinitySource | ...                   |
|     each = Searcher + Fetcher(HTTP) + Parser(HTML/JSON)   |
+-----------------------------------------------------------+
|  Infrastructure                                           |
|   HTTP client | HTML/JSON parsers | SQLite persistence    |
+-----------------------------------------------------------+
```

## Current modules

| Directory | Namespace | Responsibility |
|---|---|---|
| `include/pfdb/` | `pfdb` | Public domain value types (`Film`, `Person`, …). |
| `src/domain/` | `pfdb` | Small bits of domain logic (e.g. credit-role tokens). |
| `src/db/` | `pfdb::db` | `Repository`: the only component that speaks SQL. |
| `src/model/` | `pfdb` | `CollectionModel`: the in-memory, indexed collection. |
| `src/net/` | `pfdb::net` | `IHttpClient` + cpr-backed implementation (the fetch seam). |
| `src/sources/` | `pfdb::sources` | `ISource` plugin interface, registry, and the IMDb source (fetch) + parser (pure). |
| `src/app/` | `pfdb::app` | Enrichment: layering user data onto fetched films (merge in M3). |
| `src/io/` | `pfdb` | Serialization (JSON now; CSV/XLS later). |
| `src/cli/` | `pfdb::cli` | CLI11 front-end. |

## Key decisions

- **`Film` is a plain value.** It can be copied, compared, and serialized.
  Persistence and indexing act *on* films; they don't live inside them.
- **`Repository` owns all SQL.** Higher layers never see SQLite. This keeps the
  storage format swappable and the rest of the code testable without a database
  (or against an in-memory one via `":memory:"`).
- **Load-all into memory.** `CollectionModel::load()` reads the whole collection
  once so that filtering/sorting/searching run against RAM. `Repository` groups
  related rows in a single pass per table to avoid N+1 queries.
- **Source plugins.** Each website implements `ISource` (`search` / `fetch`).
  Fetching goes through the injectable `net::IHttpClient`; parsing is done by
  pure free functions so it can be unit-tested against saved fixtures with a mock
  client. IMDb is the first source. See [sources/writing-a-source.md](sources/writing-a-source.md)
  and [testing.md](testing.md).

## Data flow: `pfdb add`

```
CLI (parse args) → AddArgs → Film → Repository::insert
                                        └─ writes films + related rows in one txn
                                        └─ returns new id
CLI reads it back (Repository::find) → to_json → stdout
```

## Planned services

- **Enrichment** will take a source id (e.g. IMDb `tt…`), fetch and parse it into
  a partial `Film`, and merge across sources with per-field provenance.
- **Update** will check GitHub Releases and self-replace the binary.

Both get their own design docs when implemented.
