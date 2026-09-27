#!/usr/bin/env python3
"""Package a PFDB build for a GitHub Release and emit its update manifest.

Given a directory holding a built PFDB install (both executables plus their
runtime DLLs), this produces:

  * ``pfdb-<version>-<platform>.zip`` — every file in the install dir, zipped at
    the top level (so the updater can extract straight over the install).
  * ``manifest.json`` — the metadata the self-updater reads: version, release
    date, notes, and, per platform, the archive's filename, size, SHA-256 and
    download URL.

Both are written to ``--out-dir``. Upload *both* as assets of the GitHub Release
tagged ``v<version>``; the app fetches ``manifest.json`` from
``releases/latest/download/manifest.json``.

Typical use (Windows, from a Developer prompt after a Release build)::

    python tools/make_release.py \
        --install-dir build/windows-msvc/src \
        --version 0.2.0 \
        --notes-file RELEASE_NOTES.md \
        --out-dir dist

The manifest can carry multiple platforms: run this once per platform with the
same ``--out-dir`` and pass ``--merge`` on the later runs to append to the
existing ``manifest.json`` instead of overwriting it.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import platform as _platform
import sys
import zipfile
from pathlib import Path


def detect_platform() -> str:
    system = _platform.system().lower()
    machine = _platform.machine().lower()
    arch = "x64" if machine in ("amd64", "x86_64") else (
        "arm64" if machine in ("arm64", "aarch64") else machine)
    if system.startswith("win"):
        return f"windows-{arch}"
    if system == "darwin":
        return f"macos-{arch}"
    if system == "linux":
        return f"linux-{arch}"
    return f"{system}-{arch}"


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def zip_dir(install_dir: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for root, _dirs, files in os.walk(install_dir):
            for name in files:
                full = Path(root) / name
                z.write(full, full.relative_to(install_dir))


def exe_names(platform_token: str) -> tuple[str, str]:
    if platform_token.startswith("windows"):
        return "pfdb.exe", "pfdb-gui.exe"
    return "pfdb", "pfdb-gui"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--install-dir", required=True, type=Path,
                    help="Directory with the built exes + runtime DLLs")
    ap.add_argument("--version", required=True, help="Semantic version, e.g. 0.2.0")
    ap.add_argument("--out-dir", default="dist", type=Path, help="Output directory")
    ap.add_argument("--platform", default=detect_platform(),
                    help="Platform token (default: this machine's)")
    ap.add_argument("--repo", default="edelkas/pfdb", help="GitHub owner/name")
    ap.add_argument("--notes-file", type=Path, help="File with the update notes")
    ap.add_argument("--merge", action="store_true",
                    help="Append to an existing manifest.json instead of overwriting")
    args = ap.parse_args(argv)

    install_dir: Path = args.install_dir
    if not install_dir.is_dir():
        print(f"error: install dir '{install_dir}' not found", file=sys.stderr)
        return 2

    cli_exe, gui_exe = exe_names(args.platform)
    for exe in (cli_exe, gui_exe):
        if not (install_dir / exe).exists():
            print(f"warning: '{exe}' not found in {install_dir}", file=sys.stderr)

    archive_name = f"pfdb-{args.version}-{args.platform}.zip"
    archive_path = args.out_dir / archive_name
    zip_dir(install_dir, archive_path)

    asset = {
        "platform": args.platform,
        "archive": archive_name,
        "url": f"https://github.com/{args.repo}/releases/download/"
               f"v{args.version}/{archive_name}",
        "size": archive_path.stat().st_size,
        "sha256": sha256_of(archive_path),
        "cli_exe": cli_exe,
        "gui_exe": gui_exe,
    }

    notes = ""
    if args.notes_file and args.notes_file.exists():
        notes = args.notes_file.read_text(encoding="utf-8")

    manifest_path = args.out_dir / "manifest.json"
    manifest = {
        "version": args.version,
        "released": _dt.date.today().isoformat(),
        "notes": notes,
        "assets": [],
    }
    if args.merge and manifest_path.exists():
        try:
            existing = json.loads(manifest_path.read_text(encoding="utf-8"))
            if existing.get("version") == args.version:
                manifest = existing
        except json.JSONDecodeError:
            pass

    manifest["assets"] = [a for a in manifest.get("assets", [])
                          if a.get("platform") != args.platform]
    manifest["assets"].append(asset)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"Wrote {archive_path} ({asset['size']} bytes)")
    print(f"  sha256: {asset['sha256']}")
    print(f"Wrote {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
