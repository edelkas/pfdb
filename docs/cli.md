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

### `pfdb add`

Add a film manually (no online lookup yet — that arrives with the source
milestones).

| Option | Description |
|---|---|
| `--title` | **Required.** Film title. |
| `--original-title` | Original-language title. |
| `--year` | Release year. |
| `--runtime` | Runtime in minutes. |
| `--synopsis` | Plot synopsis. |
| `--genre` | A genre; repeat the flag for several. |
| `--date-watched` | Date watched, `YYYY-MM-DD`. |
| `--rating` | Your personal rating (0–10). |
| `--notes` | Free-form notes. |
| `--favorite` | Mark as a favourite. |

```sh
pfdb add --db mycollection.db --title "Blade Runner" --year 1982 \
         --genre Sci-Fi --genre Thriller --favorite
```

With `--json`, prints the stored film (including its new id) as a JSON object.

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
