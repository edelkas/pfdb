#!/usr/bin/env python3
"""Generate the small EMDB test fixture (tests/fixtures/emdb/sample.dat).

EMDB writes UTF-16LE (BOM) text that looks like JSON but isn't strict: field
groups within a movie are joined by the 0x1E record separator, file paths keep
raw backslashes, literal quotes are written as the <DQ> tag, and plots may hold
raw newlines. This script emits exactly that shape so the parser tests exercise
the real quirks, but tiny (2 movies) instead of the multi-megabyte export.

Run from the repo root:  python tools/make_emdb_fixture.py
"""

import os

RS = "\x1e"  # record separator between fields in a group


def group(*fields: str) -> str:
    return RS.join(fields)


# Movie 1: Blade Runner -- exercises AKA, collection id, custom genre code,
# cast + characters, hex ratings, personal bitmask + play count, a video path
# with backslashes, a <DQ> tag in comments, and a raw newline in the plot.
movie1 = {
    # title, also-known-as, studio, collection-id
    "title": group("Blade Runner", "Blade Runner: The Final Cut", "Warner Bros.", "1000"),
    # year, directors, runtime, country, codec-packed(13=H.265), aspect, res(@3=1080),
    # path, edition, personal(=804 -> playcount 3, bitmask 36 = owned+favorite),
    # disks, color/fps, system, region, movie-id, writers, composers
    "year": group("1982", "0", "117", "USA", "13", "4", "@3",
                  r"I:\Movies\Blade Runner.mkv", "-1", "804", "1", "1", "0", "0",
                  "116", "0", "0"),
    # genres(A,S,T + custom '1'=Giallo), imdb-id|top250, date-added, cert, tv, ?,
    # value, trailer, last-seen(=2024-01-15), rt-path, tvdb
    "genres": group("AST1", "0083658|0", "20200101", "R", "", "", "0", "",
                    "20240115", "", ""),
    # audio formats, audio langs, custom-number, awards
    "audio": group("", "", "", "O0200"),
    # actor indexes, |-separated character names
    "cast": group("0,1", "Rick Deckard|Roy Batty"),
    "plot": "A blade runner must pursue four replicants.\nSecond line of the plot.",
    "comments": "Great <DQ>tears in rain<DQ> monologue.",
    # imdb x10 hex(51=8.1), user x10 hex(19=2.5), medium, source, features, spoken,
    # subs, votes, size-MB, rt/mc stats, ean
    "rating": group("51", "19", "00", "00", "00", "E", "", "263000", "1500",
                    "999999000", ""),
    # tagline, tag indexes, web link
    "tagline": group("Man has made his match.", "0,1", ""),
}

# Movie 2: a minimal entry -- unwatched, no cast/comments.
movie2 = {
    "title": group("Minimal Movie", "", "", "-1"),
    "year": group("2001", "", "", "", "", "", "", "", "-1", "0", "1", "0", "0",
                  "0", "200", "", ""),
    "genres": group("D", "1234567|0", "", "", "", "", "0", "", "", "", ""),
    "plot": "A minimal film.",
}


def emit_string(value: str) -> str:
    """An EMDB-style JSON string: raw content, no escaping (values never hold a
    literal double quote -- EMDB uses the <DQ> tag for that)."""
    return '"' + value + '"'


def emit_movie(m: dict) -> str:
    parts = [f'        {emit_string(k)}: {emit_string(v)}' for k, v in m.items()]
    return "{\n" + ",\n".join(parts) + "\n    }"


def main() -> None:
    text = (
        "{\n"
        '    "version": "80",\n'
        '    "custom-genres": {"00": "Giallo"},\n'
        '    "custom-languages": {},\n'
        '    "collections": [{\n'
        '        "name": "Blade Runner Collection",\n'
        '        "sort": "Blade Runner Collection",\n'
        '        "id": "1000"\n'
        "    }],\n"
        '    "tags": [\n'
        '        {"name": "Cyberpunk", "id": "0", "color": "1"},\n'
        '        {"name": "Dystopia", "id": "1", "color": "2"}\n'
        "    ],\n"
        '    "actors": [\n'
        '        {"name": "Harrison Ford", "id": "0000148"},\n'
        '        {"name": "Rutger Hauer", "id": "0000442"}\n'
        "    ],\n"
        '    "directors": [{"name": "Ridley Scott"}],\n'
        '    "writers": [{"name": "Hampton Fancher"}],\n'
        '    "composers": [{"name": "Vangelis"}],\n'
        '    "movies": [' + emit_movie(movie1) + ", " + emit_movie(movie2) + "]\n"
        "}\n"
    )

    out_path = os.path.join(
        os.path.dirname(__file__), "..", "tests", "fixtures", "emdb", "sample.dat"
    )
    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(b"\xff\xfe")  # UTF-16LE BOM
        f.write(text.encode("utf-16-le"))
    print("wrote", out_path, f"({os.path.getsize(out_path)} bytes)")


if __name__ == "__main__":
    main()
