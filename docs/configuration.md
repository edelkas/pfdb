# Configuration & field presets

PFDB keeps user-level configuration in a small JSON file, separate from any
collection database so it is shared across collections. Today it stores **field
presets** used by import and CSV export.

## Location

Resolved in this order:

1. `--config <path>` (a global option), or the `PFDB_CONFIG` environment variable.
2. `<home>/.pfdb/config.json` — `%USERPROFILE%` on Windows, `$HOME` elsewhere.
3. `pfdb.config.json` in the working directory (last resort).

A missing file is fine: the built-in presets still work, and the file is created
when you first save a preset.

## Fields

A *field* is a selectable unit of film data. Tokens are kebab-case:

```
title  original-title  year  runtime  synopsis  genres
cast  directors  writers  composers  topics  groups
imdb-id  imdb-rating  user-rating
watch-date  watch-count  owned  wishlist  favorite  comments
video-file
```

## Presets

A *preset* is a named field selection. Three are built in:

| Preset | Contents |
|---|---|
| `all` | every field |
| `userdata` | `imdb-id`, `title`, `year`, and the user fields (`watch-date`, `watch-count`, `owned`, `wishlist`, `favorite`, `user-rating`, `comments`) |
| `metadata` | everything except the user fields (i.e. redownloadable data) |

Define your own with `pfdb preset`:

```sh
pfdb preset list                                  # built-ins + your presets
pfdb preset show userdata                          # a preset's fields
pfdb preset set mine title,year,user-rating,owned  # define/replace
pfdb preset remove mine
```

Built-in names are reserved and cannot be redefined or removed. Presets are used
anywhere a `--preset` option appears:

```sh
pfdb import --emdb emdb.dat --preset userdata
pfdb list --csv --preset mine
```

`--fields <tokens>` is the ad-hoc equivalent and overrides `--preset`.
