# Roadmap

PFDB is built in milestones. Each milestone is fully tested before the next
begins. Later milestones are re-planned in detail as they're reached.

- **M1 — Foundation** *(current)*
  CMake + vcpkg build, docs, test harness, SQLite schema + migrations, in-memory
  `CollectionModel`, and a minimal CLI (`init`, `add`, `list`, `remove`).

- **M2 — Sources & enrichment**
  `ISource` interface and plugin registry. IMDb source: search, fetch, and parse
  (primarily via embedded JSON-LD / `__NEXT_DATA__`, with DOM fallback), with
  fixture-driven parser tests. `pfdb search` and `pfdb add --imdb <id>`.

- **M3 — FilmAffinity + multi-source merge**
  A second source, plus merging several sources into one `Film` with per-field
  provenance and a configurable priority policy.

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
