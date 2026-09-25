#!/usr/bin/env python3
"""Capture BoxOfficeMojo fixtures for the parser tests.

BoxOfficeMojo keys on IMDb ids. This saves the title page HTML so bom_parser can
be tested offline; refresh it when the page layout changes and inspect the diff.

Run from the repo root:  python tools/capture_bom_fixtures.py
"""

import os
import urllib.request

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                  "(KHTML, like Gecko) Chrome/124.0 Safari/537.36",
    "Accept-Language": "en-US,en;q=0.9",
}

# Blade Runner 2049 (tt1856101): budget $150M, worldwide ~$278M.
TITLES = ["tt1856101"]

OUT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "tests", "fixtures", "bom")
)


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for ttid in TITLES:
        url = f"https://www.boxofficemojo.com/title/{ttid}/"
        req = urllib.request.Request(url, headers=HEADERS)
        html = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", "replace")
        path = os.path.join(OUT_DIR, f"title_{ttid}.html")
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(html)
        print("wrote", path, f"({len(html)} chars)")


if __name__ == "__main__":
    main()
