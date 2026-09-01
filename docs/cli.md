# CLI reference

The executable is `pfdb`. Every invocation takes a subcommand.

```
pfdb [--db PATH] [--json] <command> [options]
```

## Global options

| Option | Description |
|---|---|
| `--db PATH` | Path to the collection database file. Defaults to `pfdb.db`, or the `PFDB_DATABASE` environment variable if set. |
| `--json` | Emit machine-readable JSON on stdout (for scripting/piping). |
| `--version` | Print the version and exit. |
| `-h`, `--help` | Show help (works on the top level and per subcommand). |

Global options may appear before or after the subcommand, so both
`pfdb --json list` and `pfdb list --json` work.

## Commands

### `pfdb init`

Create the database file if it does not exist and bring its schema up to date.
Safe to run repeatedly.

```sh
pfdb init --db mycollection.db
```

### `pfdb search`

Search a source for films. Prints `id  title (year)  [type]` rows (the result
count goes to stderr), or a JSON array with `--json`.

| Option | Description |
|---|---|
| `query` | **Required.** Search text (positional). |
| `--source` | Source to search. Default `imdb`. |

```sh
pfdb search "blade runner"
pfdb search "blade runner" --json | jq -r '.[] | "\(.id)\t\(.title)"'
```

### `pfdb add`

Add a film either by **fetching from a source** (`--imdb <id>`) or **manually**
(`--title …`). When fetching, the manual flags below are layered *on top of* the
fetched data (e.g. set your own `--favorite`/`--date-watched`, or override the
`--title`).

| Option | Description |
|---|---|
| `--imdb <ttID>` | Fetch from IMDb by title id (e.g. `tt0083658`). |
| `--fa <faId>` | Fetch from FilmAffinity by id (e.g. `358476`). Combine with `--imdb` to merge both (IMDb wins shared fields; FA adds Spanish title/synopsis, its rating, review count, topics, groups, relations, similarities). |
| `--title` | Film title. **Required for a manual add**; overrides the fetched title. |
| `--original-title` | Original-language title. |
| `--year` | Release year (overrides fetched). |
| `--runtime` | Runtime in minutes. |
| `--synopsis` | Plot synopsis. |
| `--genre` | A genre; repeat for several (appended to fetched genres). |
| `--date-watched` | Date watched, `YYYY-MM-DD`. |
| `--rating` | Your personal rating (0–10). |
| `--notes` | Free-form notes. |
| `--favorite` | Mark as a favourite. |
| `--dry-run` | Fetch/build the film and print it, but do not save. |

```sh
# Combine IMDb + FilmAffinity, then add your own data on top:
pfdb add --imdb tt0083658 --fa 358476 --favorite --date-watched 2024-05-01

# Preview what would be fetched, without saving:
pfdb add --imdb tt0083658 --dry-run --json

# Manual add (no lookup):
pfdb add --title "Blade Runner" --year 1982 --genre Sci-Fi --favorite
```

With `--json`, prints the resulting film (including its new id) as a JSON object.
Fetching uses the sources' own backends — see [sources/imdb.md](sources/imdb.md)
and [sources/filmaffinity.md](sources/filmaffinity.md).

Relation and similarity edges are stored only between films that are **both** in
the collection, so adding both a film and its sequel links them. Because
FilmAffinity relations are reciprocal, adding the second film records the pair;
`pfdb update` (below) backfills the first film's side.

### `pfdb update`

Re-fetch a film's sources and refresh the data that changes over time (scores,
votes, review counts, topics, relations, similarities) **without touching your
own data** (favourite, date watched, personal rating, notes, video file).

| Option | Description |
|---|---|
| `id` | Film id to update (positional). |
| `--all` | Update every film in the collection instead. |

```sh
pfdb update 1          # refresh one film
pfdb update --all      # refresh the whole collection
```

### `pfdb list`

List films in the collection. Plain output is one film per line
(`id  title (year)`); the film count is printed to stderr so it doesn't pollute
piped stdout. With `--json`, prints a JSON array of full film objects.

```sh
pfdb list --db mycollection.db --json | jq '.[] | .title'
```

### `pfdb remove`

Remove a film by id (cascades to its credits, ratings, etc.).

```sh
pfdb remove --db mycollection.db 3
```

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Success. |
| `1` | Usage error (bad arguments). |
| `2` | Runtime error (I/O, database, unexpected failure). |
| `3` | Referenced film not found. |

Scripts can branch on these. Diagnostics go to stderr; data goes to stdout.
