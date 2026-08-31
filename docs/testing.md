# Testing & fixtures

Tests use [Catch2 v3](https://github.com/catchorg/Catch2) and are registered with
CTest. Every new piece of functionality should ship with tests.

## Running

```sh
cmake --preset windows-msvc
cmake --build build/windows-msvc
ctest --preset windows-msvc          # or: ctest --test-dir build/windows-msvc --output-on-failure
```

Run the test binary directly to use Catch2 filters/tags:

```sh
build/windows-msvc/tests/pfdb_tests.exe "[db]"        # only database tests
build/windows-msvc/tests/pfdb_tests.exe --list-tests
```

## Layout

```
tests/
├─ unit/          per-module tests (test_domain, test_repository, …)
├─ fixtures/      saved raw pages for parser tests (see below)
└─ integration/   end-to-end tests against a temp database (added as features land)
```

Tags in use: `[domain]`, `[db]`, `[model]`, `[json]`. Database tests open an
in-memory database (`":memory:"`), so they need no filesystem and run isolated.

## Fixture-driven parser tests (the anti-breakage strategy)

Scraping is fragile: a website can change its markup at any time and silently
break parsing. PFDB defends against this by **separating fetching from parsing**.

- **Fetchers** perform network I/O and are not exercised in the normal test run.
- **Parsers** are pure functions from a raw document (HTML/JSON string) to a
  partial `Film`. They are tested against **saved fixtures** — real pages
  captured once and committed under `tests/fixtures/<source>/`.

When a site changes format and a parser breaks:

1. Re-capture the page into `tests/fixtures/<source>/` (a helper script will be
   provided with the first source).
2. The fixture diff shows exactly what changed.
3. Update the parser and its expected-output assertions together.

Fixtures are treated as binary (`-text` in `.gitattributes`) so line-ending
normalization never corrupts a captured page. This milestone ships no parsers
yet, so `tests/fixtures/` is a placeholder; it fills up with the IMDb source.
