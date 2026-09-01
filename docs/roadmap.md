# Roadmap

PFDB is built in milestones. Each milestone is fully tested before the next
begins. Later milestones are re-planned in detail as they're reached.

- **M1 — Foundation** *(done)*
  CMake + vcpkg build, docs, test harness, SQLite schema + migrations, in-memory
  `CollectionModel`, and a minimal CLI (`init`, `add`, `list`, `remove`).

- **M2 — Sources & enrichment** *(done)*
  `ISource` interface and plugin registry with strict fetch/parse separation.
  IMDb source via the site's own JSON backends — the suggestion API for search
  and the GraphQL API for detail (no HTML parsing needed) — with fixture-driven
  parser tests and a mock HTTP client. `pfdb search` and `pfdb add --imdb <id>`.
  See [sources/imdb.md](sources/imdb.md).

- **M3 — FilmAffinity + multi-source merge** *(current)*
  FilmAffinity source (HTML scraping via lexbor) for Spanish-specific data:
  spanish title/synopsis, FA rating, review count, topics, groups, and
  film-to-film relations & similarities. Two-source merge (IMDb wins shared
  fields), edges stored only for films both in the collection, and a `pfdb
  update` command that refreshes volatile data without touching user fields.
  See [sources/filmaffinity.md](sources/filmaffinity.md).

- **M4 — Power query engine**
  Complex filters, regex matching, multi-key sort, column selection, saved views.
  CSV export.

- **M5 — Import/export**
  Import from emdb; XLS export.

- **M6 — Rich metadata & playback**
  Cover art, financials, reviews; open-in-video-player; local video-file metadata.

- **M7 — Dear ImGui GUI**
  The graphical front-end. Interface spec provided at that point.

- **M8 — Packaging & auto-update**
  GitHub Releases packaging and self-update from within the app.

## Design commitments carried throughout

- Source-agnostic core behind `ISource`.
- Fetch/parse separation with fixture-tested parsers.
- In-memory collection for speed; SQLite single-file store.
- Tests accompany every feature.
- Documentation lives here in `docs/`, organized by topic — not crammed into the
  README.
