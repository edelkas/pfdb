# PFDB — Power Film DataBase

A power-user film collection manager, written in C++. Add movies, enrich them
with metadata scraped from sources like **IMDb** and **FilmAffinity**, keep your
own data (watch dates, ratings, local video files), and browse, search, filter,
and sort your collection with power-user tooling.

> **Status:** early development. The current build is a CLI foundation
> (SQLite-backed collection + manual add/list/remove). Online enrichment, rich
> filtering, import/export, and a Dear ImGui GUI are on the [roadmap](docs/roadmap.md).

## Quickstart

```sh
# Configure and build (Windows, MSVC + Ninja, dependencies via vcpkg)
cmake --preset windows-msvc
cmake --build build/windows-msvc

# Run
build/windows-msvc/src/pfdb.exe init --db mycollection.db
build/windows-msvc/src/pfdb.exe add  --db mycollection.db --title "Blade Runner" --year 1982
build/windows-msvc/src/pfdb.exe list --db mycollection.db --json
```

See **[docs/](docs/index.md)** for full documentation:

- [Architecture](docs/architecture.md)
- [Data model & schema](docs/data-model.md)
- [CLI reference](docs/cli.md)
- [Testing & fixtures](docs/testing.md)
- [Roadmap](docs/roadmap.md)

## Building

Requires a C++20 compiler, CMake ≥ 3.25, and [vcpkg](https://github.com/microsoft/vcpkg)
(set `VCPKG_ROOT`). Dependencies are declared in `vcpkg.json` and fetched
automatically on first configure. See [docs/index.md](docs/index.md) for details.

## License

[MIT](LICENSE).
