#!/usr/bin/env python3
"""Capture raw IMDb responses as test fixtures.

PFDB tests parse *saved* IMDb responses so they run offline and deterministically
(see docs/testing.md). When IMDb changes the shape of its JSON and a parser test
breaks, re-run this script to refresh the fixtures, then reconcile the diff with
the parser and its expected values.

Usage:
    python tools/capture_imdb_fixtures.py

Writes into tests/fixtures/imdb/. Requires only the Python standard library.

Note: this hits IMDb's own (unofficial) JSON backends -- the same ones the CLI
uses at runtime. Their responses carry a "limited non-commercial use" notice;
PFDB is a personal, non-commercial tool. Keep captures small and infrequent.
"""
import gzip
import json
import os
import urllib.parse
import urllib.request

UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36")

# The single GraphQL query PFDB uses for title detail. Keep this byte-for-byte in
# sync with kTitleQuery in src/sources/imdb/imdb_source.cpp.
TITLE_QUERY = (
    '{title(id:"%s"){id titleText{text} originalTitleText{text} '
    'titleType{text} releaseYear{year} runtime{seconds} '
    'ratingsSummary{aggregateRating voteCount} plot{plotText{plainText}} '
    'genres{genres{text}} primaryImage{url} '
    'principalCredits{category{text} credits{name{id nameText{text}}}}}}'
)

FIXTURES_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "tests", "fixtures", "imdb")


def _read(resp):
    body = resp.read()
    if resp.headers.get("Content-Encoding") == "gzip":
        body = gzip.decompress(body)
    return body


def get_suggestion(query):
    url = ("https://v3.sg.media-imdb.com/suggestion/x/%s.json?includeVideos=0"
           % urllib.parse.quote(query))
    req = urllib.request.Request(
        url, headers={"User-Agent": UA, "Accept-Encoding": "gzip"})
    with urllib.request.urlopen(req, timeout=25) as r:
        return _read(r)


def get_title(imdb_id):
    body = json.dumps({"query": TITLE_QUERY % imdb_id}).encode()
    req = urllib.request.Request(
        "https://caching.graphql.imdb.com/", data=body,
        headers={"User-Agent": UA, "Accept-Encoding": "gzip",
                 "Content-Type": "application/json",
                 "Origin": "https://www.imdb.com",
                 "Referer": "https://www.imdb.com/"})
    with urllib.request.urlopen(req, timeout=25) as r:
        return _read(r)


def save(name, data):
    os.makedirs(FIXTURES_DIR, exist_ok=True)
    path = os.path.join(FIXTURES_DIR, name)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (path, len(data)))


def main():
    save("suggestion_blade_runner.json", get_suggestion("blade runner"))
    save("title_tt0083658.graphql.json", get_title("tt0083658"))
    # A well-formed request for a non-existent id: data.title comes back null.
    save("title_notfound.graphql.json", get_title("tt00000000"))


if __name__ == "__main__":
    main()
