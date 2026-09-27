# Updating & packaging

PFDB can keep itself current: it watches its GitHub Releases, detects a newer
version, verifies it, and swaps it into place — for both `pfdb` (CLI) and
`pfdb-gui` (GUI), which ship together in one package.

## The release manifest

Each release carries a `manifest.json` asset describing it:

```json
{
  "version": "0.2.0",
  "released": "2026-10-01",
  "notes": "What changed in this release...",
  "assets": [
    {
      "platform": "windows-x64",
      "archive": "pfdb-0.2.0-windows-x64.zip",
      "url": "https://github.com/edelkas/pfdb/releases/download/v0.2.0/pfdb-0.2.0-windows-x64.zip",
      "size": 9815203,
      "sha256": "81987918ec819c495c2d9c26ddd6c88f6afc1b043826278976a234d5db43358d",
      "cli_exe": "pfdb.exe",
      "gui_exe": "pfdb-gui.exe"
    }
  ]
}
```

PFDB fetches it from the stable URL
`https://github.com/<repo>/releases/latest/download/manifest.json` (which GitHub
redirects to the latest release), so no API token or authentication is needed.
Versions follow [semantic versioning](https://semver.org); a release is "newer"
strictly by SemVer precedence (a pre-release sorts before its release).

## What happens on an update

The pipeline is deliberately fail-safe — the live install is never touched until
every check has passed:

1. **Check** — fetch the manifest; if its version isn't newer, stop.
2. **Download** — get the platform's archive into a staging dir beside the
   install (`pfdb-update/`).
3. **Verify integrity** — the archive's **size and SHA-256** must match the
   manifest. On any mismatch the download is discarded.
4. **Unpack** — extract the archive into `pfdb-update/unpacked/`.
5. **Version check** — run the extracted `pfdb --version` and `pfdb-gui --version`
   and confirm they report the manifest's version. If not, discard.
6. **Swap** — because Windows locks the DLLs a running process has loaded, the
   swap is done by a short helper step re-executed *from the unpacked copy*
   (`pfdb __apply-update …`). After the current process exits it renames each
   existing file to `*.old`, moves the new files in (rolling back the renames if
   anything fails), records the outcome, and relaunches PFDB.
7. **First run** — the new version, on its first launch, deletes the leftover
   `*.old` files and the staging dir, and shows the update notes once (or an
   error message if the swap had to roll back). This "first run" state is tracked
   in the config (`last_run_version`), so the modal appears exactly once.

If a pre-swap check fails, the update is simply discarded and the current install
keeps running. If the swap itself fails mid-way, the renames are rolled back to
the previous binaries.

## Using it

### CLI

```sh
pfdb upgrade                 # check, then download + install if newer (asks first)
pfdb upgrade --check         # only report whether an update is available
pfdb upgrade --yes           # install without the confirmation prompt
pfdb upgrade --interval 24   # set the auto-check cadence (hours; 0 disables)
```

Every ordinary command also does a lightweight, **notify-only** check at most
once per `check_interval_hours` (default: daily): if a newer version exists it
prints a one-line notice to stderr and leaves installing to you. It never slows
scripting down beyond that one throttled check, and network failures are silent.

### GUI

The toolbar's **Check for updates** button runs the check on a background thread;
when one is found it becomes **Update available!** and opens a dialog with the
version and notes. **Update now** downloads and verifies in the background, then
hands off to the swapper and relaunches. The auto-check also runs on launch when
due. The cadence lives in **Settings**, and the first launch after a successful
update shows the notes (or an error) in a modal.

## Cutting a release (maintainers)

`tools/make_release.py` packages a built install and writes the manifest:

```sh
python tools/make_release.py \
    --install-dir build/windows-msvc/src \
    --version 0.2.0 \
    --notes-file RELEASE_NOTES.md \
    --out-dir dist
```

It zips the executables and their runtime DLLs at the archive's top level,
computes the size and SHA-256, and emits `dist/manifest.json`. Upload **both**
the zip and `manifest.json` as assets of the `v0.2.0` GitHub Release. Run it once
per platform with the same `--out-dir` and `--merge` to add more platforms to one
manifest.

The workflow `.github/workflows/release.yml` automates this: push a tag like
`v0.2.0` and it builds, packages, and attaches the assets to the Release.
