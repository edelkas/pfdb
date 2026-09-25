# IMDb source

The IMDb source provides search and full title metadata. IMDb aggressively
blocks naive scraping of its HTML pages and GraphQL endpoint (a plain request
gets an empty `202` or a `403`), but two of the site's **own JSON backends** —
the same ones imdb.com's pages call — are reachable from an ordinary HTTP client.
PFDB uses those. Both return JSON, so **no HTML parsing is involved**.

> **Terms of use.** IMDb's GraphQL responses carry a notice that the data is for
> "limited non-commercial use" only. PFDB is a personal, non-commercial tool and
> uses these endpoints on that basis. This is IMDb's *unofficial* backend; it can
> change without notice, which is exactly why parsing is covered by fixtures
> (see [refreshing fixtures](#refreshing-fixtures)).

## Endpoints

### Search — suggestion API

```
GET https://v3.sg.media-imdb.com/suggestion/x/{url-encoded query}.json?includeVideos=0
```

Returns `{"d": [ ... ]}`. Each entry PFDB keeps (id begins `tt`) maps to a
`SearchResult`:

| JSON field | SearchResult |
|---|---|
| `id` | `external_id` (e.g. `tt0083658`) |
| `l` | `title` |
| `y` | `year` |
| `q` | `type` (`feature`, `TV series`, …) |
| `s` | `subtitle` (top-billed cast) |
| `i.imageUrl` | `image_url` |

Person (`nm…`) and video (`vi…`) suggestions are skipped.

### Detail — GraphQL

```
POST https://caching.graphql.imdb.com/
Content-Type: application/json
Origin:  https://www.imdb.com
Referer: https://www.imdb.com/
Body: {"query": "<kTitleQuery>"}
```

The `Origin`/`Referer` headers are required — without them the endpoint returns
`403`. The exact query is `kTitleQuery` in
`src/sources/imdb/imdb_source.cpp` (kept byte-for-byte identical to `TITLE_QUERY`
in `tools/capture_imdb_fixtures.py`). Field mapping into `Film`:

| GraphQL path | Film field |
|---|---|
| `titleText.text` | `title` |
| `originalTitleText.text` | `original_title` |
| `releaseYear.year` | `year` |
| `runtime.seconds` ÷ 60 | `runtime_minutes` |
| `plot.plotText.plainText` | `synopsis` |
| `genres.genres[].text` | `genres` |
| `ratingsSummary.aggregateRating` / `voteCount` | `ratings[0]` (`source="imdb"`, `scale=10`) |
| `principalCredits[]` → `credits[].name.nameText.text` | `credits` (Director→Director, Writers→Writer, Stars→Actor) |
| title `id` | `source_refs[0]` (`fetched_at` stamped by the fetcher) |

`primaryImage.url` is parsed and carried on the fetch result as the film's cover
URL (capped to ~640px wide); `pfdb add --cover` downloads it into the `covers`
table. The title `type` is fetched but not yet stored.

### Not-found

A well-formed request for a non-existent id returns a `title` object whose
`titleText` is `null` (not a top-level `null`). The parser treats an empty title
text as not-found and raises `SourceError{NotFound}`.

## Refreshing fixtures

Parser tests run against saved responses under `tests/fixtures/imdb/`. When IMDb
changes shape and a test breaks:

```sh
python tools/capture_imdb_fixtures.py     # rewrites the fixtures
```

Then reconcile the diff with `imdb_parser.cpp` and the expected values in
`tests/unit/test_imdb_parser.cpp`. See [../testing.md](../testing.md).

## Live check

A hidden, network-touching test verifies the whole path against the real site:

```sh
pfdb_tests "[.imdb-live]"
```

It is excluded from the default `ctest` run so CI stays offline and deterministic.
