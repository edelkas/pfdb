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
| `src/model/` | `pfdb` | `CollectionModel`: the in-memory, indexed collection, plus its relation/similarity edge adjacency. |
| `src/query/` | `pfdb::query` | The query engine: field registry, filters, the boolean-expression AST, the parsers, name→id normalization, sort, and the `run_query` entry point. |
| `src/net/` | `pfdb::net` | `IHttpClient` + cpr-backed implementation (the fetch seam). |
| `src/parse/` | `pfdb::parse` | `HtmlDocument`: a lexbor-backed HTML/CSS-selector wrapper for HTML sources. |
| `src/sources/` | `pfdb::sources` | `ISource` plugin interface, registry, and the IMDb (JSON) + FilmAffinity (HTML) + BoxOfficeMojo (HTML, financials) sources — each a fetcher plus pure parsers. |
| `src/media/` | `pfdb::media` | `probe()`: local video-file metadata via MediaInfo (libmediainfo). |
| `src/app/` | `pfdb::app` | Enrichment (two-source merge; layering user data) and import orchestration (`import_emdb`, upsert by IMDb id). |
| `src/io/` | `pfdb` | Serialization (JSON, CSV export), the import/export `FieldSet`, and the EMDB reader (`io/emdb/`: decode → parse → map). |
| `src/app/` (config) | `pfdb::config` | `Config`: user-level JSON config holding field presets. |
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

## Data flow: `pfdb list` (querying)

```
CLI (--filter/--where/--sort) → query::QueryRequest
  → run_query(model, req):
      parse each --filter   → query::Filter        (parser.cpp)
      parse --where         → boolean Expr tree     (parser.cpp; extended ops reduced)
      normalize             → cast/crew names → id filters, using the model's
                              people index          (normalize.cpp)
      evaluate over model.all() (short-circuit AND/OR)   (expr.cpp / filter.cpp)
      sort                  → stable multi-key comparator (sort.cpp)
  → vector<const Film*> → JSON / CSV / rows
```

The query engine is pure and source-agnostic: it reads `Film` values and two
edge-adjacency maps (`related_to`/`similar_to`) held by the `CollectionModel`,
and never touches SQL or the network. Fields are defined in one registry
(`field.cpp`), so new queryable/sortable fields are a one-line addition.

## Planned services

- **Enrichment** will take a source id (e.g. IMDb `tt…`), fetch and parse it into
  a partial `Film`, and merge across sources with per-field provenance.
- **Update** will check GitHub Releases and self-replace the binary.

Both get their own design docs when implemented.
