# Test fixtures

Saved raw pages (HTML / JSON) captured from real sources, used to test parsers
offline and deterministically. See [../../docs/testing.md](../../docs/testing.md).

Organized by source once parsers exist, e.g.:

```
fixtures/
├─ imdb/
│   └─ tt0083658.title.html
└─ filmaffinity/
    └─ 476334.film.html
```

Fixtures are committed as-is and treated as binary (`-text` in `.gitattributes`)
so line-ending normalization never alters a captured page. This directory is a
placeholder until the first source (IMDb) lands in M2.
