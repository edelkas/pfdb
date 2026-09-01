# FilmAffinity source

FilmAffinity provides Spanish-specific data that complements IMDb: the Spanish
title and synopsis, the FA score/votes/review-count, finer-grained **topics**,
**groups/sagas**, and film-to-film **relations** and **similarities**. Unlike
IMDb, FilmAffinity has no JSON API — its pages are plain HTML, parsed with the
lexbor-backed [`parse/html`](../../src/parse/html.hpp) wrapper.

> **Terms of use.** FilmAffinity pages carry a limited-use notice; PFDB is a
> personal, non-commercial tool. As with any HTML source this can change shape
> at any time, which is why parsing is covered by fixtures.

## Field policy (vs IMDb)

FilmAffinity supplies only fields that **don't** overlap IMDb; IMDb wins every
shared field (title, original title, year, runtime, English synopsis, genres,
cast/crew). See [../../src/app/enrichment.cpp](../../src/app/enrichment.cpp).

| PFDB field | From FilmAffinity |
|---|---|
| `spanish_title`, `spanish_synopsis` | title / synopsis |
| a second `filmaffinity` rating (value + votes) | the FA score |
| `review_count` | número de críticas |
| `topics` | "temas" (finer than genres) |
| `groups` | groups / sagas |
| relations, similarities | the edge pages/section below |

## Endpoints & selectors

All under `https://www.filmaffinity.com/es`. Requests send a browser User-Agent
and `Accept-Language: es-ES`.

### Search — `search.php?stext={query}`

Results are grouped by year (`li.se-it` with a `.group-by.year` header); each
title anchor points at `film<ID>.html`. A single strong match redirects straight
to the film page, which the parser recovers via `meta[property=og:url]`.

### Film — `film{ID}.html`

| Data | Selector |
|---|---|
| Spanish title | `#main-title [itemprop=name]` |
| Spanish synopsis | `[itemprop=description]` |
| Score | `[itemprop=ratingValue]@content` |
| Votes | `[itemprop=ratingCount]@content` |
| Review count | `[itemprop=reviewCount]@content` |
| Topics | `a[href*=movietopic.php]` |
| Groups | `a[href*=movie-group.php]` |
| Similar movies | `li.slider-item[data-movie-id]` + inner `animate@to` = percent |

Genres (`span[itemprop=genre]`) are intentionally ignored — IMDb wins.

### Relations — `movie-relations.php?movie-id={ID}`

Related films are grouped in `div.fa-content-card` blocks, each led by a
`div.card-header` giving the Spanish relationship label ("tiene secuela",
"tiene reboot", "documental asociado", "está relacionada con", "serie
relacionada", "Videojuego relacionado"). Every `[data-movie-id]` under a card
becomes a relation with that label as its `kind`.

## How edges are stored

Relations and similarities are edges between **collection** films, resolved by
looking up each FA id via `Repository::find_id_by_source_ref("filmaffinity", …)`.
Only pairs where **both** films are already in the database are stored; a film is
never created just because it appears in another's relation/similar list. Because
FA relations are reciprocal, adding the second film of a pair records the link;
`pfdb update` backfills anything missed. Relations are directed (labelled from
the source film's side); similarities are undirected with a percentage.

## Refreshing fixtures

```sh
python tools/capture_fa_fixtures.py    # search, film, and relations pages
```

Then reconcile the diff with `fa_parser.cpp` and the expectations in
`tests/unit/test_fa_parser.cpp`. A hidden live check exercises the real site:

```sh
pfdb_tests "[.fa-live]"
```
