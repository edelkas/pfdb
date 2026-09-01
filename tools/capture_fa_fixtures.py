#!/usr/bin/env python3
"""Capture raw FilmAffinity responses as test fixtures.

PFDB parses *saved* FilmAffinity pages so tests run offline (see
docs/testing.md). When FilmAffinity changes its markup and a parser test breaks,
re-run this script to refresh the fixtures, then reconcile the diff with the
parser and its expected values.

Usage:
    python tools/capture_fa_fixtures.py

Writes into tests/fixtures/fa/. Standard library only.

FilmAffinity is a Spanish HTML site (no API); we send a browser User-Agent and
Spanish Accept-Language. Its pages carry a "limited non-commercial use" notice;
PFDB is a personal, non-commercial tool. Keep captures small and infrequent.
"""
import gzip
import os
import urllib.parse
import urllib.request

UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36")

BASE = "https://www.filmaffinity.com/es"
FIXTURES_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "tests", "fixtures", "fa")


def get(url):
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept-Language": "es-ES,es;q=0.9,en;q=0.8",
        "Accept-Encoding": "gzip",
    })
    with urllib.request.urlopen(req, timeout=25) as r:
        body = r.read()
        if r.headers.get("Content-Encoding") == "gzip":
            body = gzip.decompress(body)
        return body


def save(name, data):
    os.makedirs(FIXTURES_DIR, exist_ok=True)
    path = os.path.join(FIXTURES_DIR, name)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (path, len(data)))


def main():
    # Blade Runner = FA id 358476.
    save("search_blade_runner.html",
         get(BASE + "/search.php?stext=" + urllib.parse.quote("blade runner")))
    save("film358476.html", get(BASE + "/film358476.html"))
    save("relations_358476.html", get(BASE + "/movie-relations.php?movie-id=358476"))


if __name__ == "__main__":
    main()
