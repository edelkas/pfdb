# PFDB Documentation

Welcome to the PFDB documentation. PFDB (Power Film DataBase) is a C++ film
collection manager aimed at power users.

## Contents

- **[Architecture](architecture.md)** — the layered design and how the pieces fit.
- **[Data model & schema](data-model.md)** — the domain types and the SQLite schema.
- **[CLI reference](cli.md)** — every command and option.
- **[Querying](querying.md)** — filters, boolean expressions, and sorting for `pfdb list`.
- **[Testing & fixtures](testing.md)** — how tests are organized and how to keep
  scraping parsers honest with saved fixtures.
- **[Roadmap](roadmap.md)** — milestones from the CLI foundation to the GUI.

## Building from source

### Prerequisites

- A C++20 compiler (MSVC 19.3x / VS 2022, Clang 16+, or GCC 12+).
- [CMake](https://cmake.org/) ≥ 3.25 and [Ninja](https://ninja-build.org/).
  On Windows these ship inside Visual Studio 2022.
- [vcpkg](https://github.com/microsoft/vcpkg): clone it, run its bootstrap
  script, and set the `VCPKG_ROOT` environment variable to its path.

### Configure & build

PFDB uses CMake presets that wire in the vcpkg toolchain automatically:

```sh
cmake --preset windows-msvc      # or windows-clang
cmake --build build/windows-msvc
ctest --preset windows-msvc      # run the test suite
```

Dependencies (SQLite, CLI11, spdlog, nlohmann-json, Catch2) are declared in
`vcpkg.json` and built on first configure. The build produces:

- `build/<preset>/src/pfdb.exe` — the CLI.
- `build/<preset>/tests/pfdb_tests.exe` — the test runner.

## Design principles

PFDB is built around a few commitments that shape everything else:

1. **Source-agnostic core.** Websites are plugins behind an `ISource` interface;
   the core knows nothing about IMDb specifically.
2. **Fetch/parse separation.** Network I/O is isolated from parsing, so parsers
   are pure functions tested against saved page fixtures — the defense against
   websites silently changing their markup.
3. **In-memory speed.** The whole collection loads into memory for fast
   filtering, sorting, and searching; SQLite is the durable single-file store.
4. **Everything is tested.** New functionality ships with tests; scraping code
   especially.
