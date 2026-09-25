# Querying the collection

`pfdb list` filters, combines, and sorts the collection entirely in memory. A
query has three stages, mirrored in the code (`src/query/`):

1. **Parse** each `--filter` spec into a filter, and the `--where` string into a
   boolean expression tree.
2. **Normalize** — reduce extended boolean operators to AND/OR/NOT, and rewrite
   name-based cast/crew inclusion into concrete id filters.
3. **Query** — evaluate the expression over every film, then sort.

```
pfdb list [--filter SPEC]... [--where EXPR] [--sort SPEC] [--json | --csv]
```

With no options, `list` prints the whole collection in load order, exactly as
before.

## Filters

Each `--filter` (short `-f`) defines one filter, written `FIELD OP VALUE`. In
`--where`, filters are referred to by number in the order they were given: the
first `--filter` is `F1`, the second `F2`, and so on.

There are four filter kinds, chosen by the operator (and validated against the
field's type):

| Kind | Operators | Meaning |
|---|---|---|
| Textual | `~`, `=~` | `~` = case-insensitive substring (the fast path, no regex engine); `=~` = regex (ECMAScript, case-insensitive) |
| Numeric | `=`, `>`, `>=`, `<`, `<=` | compare a number; `=` also takes a range `A..B` |
| Date | `=`, `>`, `>=`, `<`, `<=` | same, over ISO `YYYY-MM-DD` dates (compared chronologically) |
| Inclusion | `has` | the field's list contains the value |

Ranges: `year = 1940..1949` is the closed range \[1940, 1949]; a bound may be
omitted for an open end (`year = 2000..`). `>` and `<` are strict; `>=`, `<=`,
and `=` (single value) are inclusive.

Quote a value with spaces: `cast has "Harrison Ford"`. `has` must be surrounded
by spaces; the symbolic operators need not be.

### Fields

| Field(s) | Type | Notes |
|---|---|---|
| `title`, `original_title`, `spanish_title`, `synopsis`, `spanish_synopsis`, `notes` | text | `~` / `=~` |
| `year`, `runtime`, `my_rating`, `watch_count`, `review_count`, `imdb_rating`, `fa_rating`, `budget`, `gross` | number | ranges/compares |
| `date_watched` | date | ranges/compares |
| `genre`, `topic`, `group` | string list | `has` = exact (case-insensitive) member |
| `cast`, `director`, `writer` | name list | `has` = name match → ids (see below) |
| `cast_id`, `director_id`, `writer_id` | id list | `has` = exact person id |
| `related_to`, `similar_to` | id list | `has` = a related/similar **film id** |

Field names are case-insensitive and tolerate a hyphen or a trailing plural
(`original-title`, `genres`). `my_rating` is your personal rating; `imdb_rating`
/ `fa_rating` are the sourced scores. Budget and box-office fields will slot into
this same table once a source provides them.

### How cast/crew inclusion works

Inclusion is fundamentally an **exact id** test — `cast_id has 42` selects films
whose cast includes person 42. Because you normally know a name, not an id,
`cast`/`director`/`writer` accept a name and are rewritten during normalization:

```
cast has "John"
   → find every actor whose name contains "John"  (John Wayne #10, John Ford #11, …)
   → (cast_id has 10) OR (cast_id has 11) OR …
```

So one name naturally expands to all the people it matches (searching `Hugh`
finds Jackman, Grant, and Laurie). A name that matches nobody yields no results.
Finding a film's relatives or look-alikes is the same inclusion idea over film
ids: `related_to has 1`, `similar_to has 1`.

## Combining filters: `--where`

`--where` (short `-w`) is a boolean expression over the filter references. With
no `--where`, all filters are combined with AND (every filter must hold).

```sh
pfdb list -f 'title ~ blade' -f 'year >= 1980' -f 'cast has "Harrison Ford"' \
          -w 'F1 AND (F2 OR F3)'
```

- **Operators:** `NOT`, `AND`, `OR`, plus the derived `NAND`, `NOR`, `XOR`,
  `XNOR` (`EQUIV`), `IMPLY`, `NIMPLY`. The derived ones are reduced to
  NOT/AND/OR internally, so the evaluator only ever handles the basic three.
- **Aliases:** `!` = NOT, `&&` = AND, `||` = OR. Word operators are
  case-insensitive.
- **Precedence** (highest first), overridable with parentheses:

  `NOT` › `AND`, `NAND` › `XOR`, `XNOR` › `OR`, `NOR` › `IMPLY`, `NIMPLY`

  So `F1 OR F2 AND F3` means `F1 OR (F2 AND F3)`.

## Sorting: `--sort`

`--sort` (short `-s`) is a comma-separated list of `field[:dir]`, where `dir` is
`asc` (default) or `desc`. Earlier keys are more significant; sorting is stable,
so unspecified ties keep load order. Only scalar fields (text/number/date) can
be sorted. Films with an **unset** value for a key sort **last**, whichever the
direction.

```sh
# Reverse-chronological, breaking ties alphabetically by title.
pfdb list --sort 'year:desc,title:asc'
```

## Output

- Default: one `id  title (year)` row per film (count to stderr).
- `--json`: a JSON array of full film objects (with `relations`/`similarities`).
- `--csv`: RFC-4180 CSV to stdout — `id,title,original_title,year,runtime,genres,
  imdb_rating,fa_rating,date_watched,favorite,my_rating`. `--json` and `--csv`
  are mutually exclusive.

A malformed filter, expression, or sort spec exits `1` (usage error) with a
message on stderr.

## Worked examples

```sh
# Film noir from the 1940s, best first.
pfdb list -f 'genre has Noir' -f 'year = 1940..1949' --sort 'imdb_rating:desc'

# Anything with Harrison Ford that you rated at least 8, as CSV.
pfdb list -f 'cast has "Harrison Ford"' -f 'my_rating >= 8' -w 'F1 AND F2' --csv

# Long sci-fi directed by Villeneuve, or anything you marked as a favourite.
pfdb list -f 'genre has Sci-Fi' -f 'runtime >= 150' -f 'director has Villeneuve' \
          -w '(F1 AND F2 AND F3)'

# Films watched in 2024, newest first.
pfdb list -f 'date_watched = 2024-01-01..2024-12-31' --sort 'date_watched:desc'

# Everything related to film #1.
pfdb list -f 'related_to has 1'
```
