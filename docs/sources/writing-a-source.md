# Writing a source

A **source** is a website PFDB pulls film data from (IMDb, FilmAffinity, …).
Sources are plugins behind one interface, so the rest of the app never depends on
any particular site.

## The contract: `ISource`

Defined in `src/sources/source.hpp`:

```cpp
class ISource {
public:
    virtual std::string id() const = 0;            // "imdb"
    virtual std::string display_name() const = 0;  // "IMDb"
    virtual std::vector<SearchResult> search(std::string_view query) = 0;
    virtual Film fetch(std::string_view external_id) = 0;
};
```

- `search` returns lightweight `SearchResult`s (id, title, year, type, …) for a
  pick-list.
- `fetch` returns a fully-populated `Film` for one `external_id`.
- On failure, throw `SourceError` with a `Kind` (`NotFound` / `Network` /
  `Parse`); the CLI maps that to an exit code.

## The rule: separate fetching from parsing

This is the project's core defense against websites changing their markup.

- **Fetching** (network I/O) goes through `net::IHttpClient`. Your source holds a
  reference to one; it never calls libcurl/cpr directly. This is what lets tests
  inject `MockHttpClient` and run offline.
- **Parsing** lives in **pure free functions** — raw response text in, domain
  objects out, no I/O, no clock. IMDb's are `parse_suggestions` and `parse_title`
  in `src/sources/imdb/imdb_parser.{hpp,cpp}`. Anything time- or network-derived
  (like `fetched_at`) is stamped by the source *after* parsing, so the parser
  stays deterministic.

Parsers are tested against **saved fixtures** (`tests/fixtures/<source>/`). When
the site changes, refresh the fixture, read the diff, fix the parser.

## Steps to add a source

1. Create `src/sources/<name>/` with a `<name>_parser.{hpp,cpp}` (pure) and a
   `<name>_source.{hpp,cpp}` (implements `ISource`, owns fetching).
2. Register it in `src/sources/source_registry.cpp` (`make_source` +
   `available_sources`).
3. Add the new `.cpp` files to `pfdb_core` in `src/CMakeLists.txt`.
4. Capture real responses into `tests/fixtures/<name>/` (add a
   `tools/capture_<name>_fixtures.py` helper) and write parser tests plus a
   mock-driven source test. Add a hidden `[.<name>-live]` test for the real site.
5. Document the endpoints and field mapping in `docs/sources/<name>.md`.

See [imdb.md](imdb.md) for a pure-JSON example and [filmaffinity.md](filmaffinity.md)
for an HTML one. HTML sources parse with the lexbor-backed `parse::HtmlDocument`
(CSS selectors — remember to quote attribute values containing `.` or `:`, e.g.
`a[href*="movietopic.php"]`), and a source may fetch more than one page per title
(FilmAffinity fetches the film page and a separate relations page) and return
film-to-film edges in its `SourceFetch`. The fetch/parse split and fixture testing
stay exactly the same.
