# BoxOfficeMojo source (financials)

[BoxOfficeMojo](https://www.boxofficemojo.com/) provides a film's **budget** and
**box-office gross**. Conveniently, it keys on the **same ids as IMDb**
(`boxofficemojo.com/title/<ttid>/`), so PFDB can fetch financials for any film it
already knows from IMDb without a separate lookup.

It is an HTML page (no API), scraped with the lexbor-backed `parse::HtmlDocument`
like FilmAffinity. A browser `User-Agent` is required (bare clients get an empty
page).

## What it contributes

Only two fields, both in USD, stored on `Film`:

| Field | Where on the page |
|---|---|
| `budget` | the labelled summary row: a `div.a-section.a-spacing-none` whose first `<span>` is "Budget", value in its `.money` span |
| `gross` (worldwide total) | the last `.money` in `.mojo-performance-summary-table` (the Domestic / International / **Worldwide** summary, in that order) |

Money strings like `$150,000,000` are reduced to integer dollars. Missing figures
are simply left unset — this is scraping, so the parser is
[fixture-tested](../testing.md) (`tests/fixtures/bom/`, refreshed by
`tools/capture_bom_fixtures.py`) and both figures are optional.

## How it's used

BoxOfficeMojo is a registered source (`boxofficemojo`), but because it shares
IMDb's ids it's driven through a convenience flag rather than a second id:

```sh
pfdb add --imdb tt1856101 --financials     # fetch budget/gross with the IMDb id
```

This stores a `boxofficemojo` source-ref alongside the IMDb one, so
`pfdb update` re-fetches the financials like any other source — keeping them
fresh as box-office numbers climb. Budget and gross are queryable
(`budget`, `gross`) and CSV-exportable; see [../querying.md](../querying.md).

BoxOfficeMojo carries a "limited non-commercial use" ToS like the other sources;
same personal-use basis. See [writing-a-source.md](writing-a-source.md) for the
source contract.
