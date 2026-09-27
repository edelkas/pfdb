# The GUI (`pfdb-gui`)

`pfdb-gui` is the graphical front-end, built with **Dear ImGui** on a **GLFW +
OpenGL3** backend. It is a second executable beside the CLI and shares the same
`pfdb_core`, database, and config — so everything the CLI can do to the data, the
GUI reflects.

```sh
pfdb-gui [--db PATH]      # defaults to $PFDB_DATABASE, else pfdb.db
```

It reads `PFDB_CONFIG` for field presets, exactly like the CLI.

## Layout

A single window fills the viewport. A **toolbar** runs along the top; below it a
draggable **splitter** divides a **table panel** from a **detail panel**. The
splitter is horizontal (table on top) by default; the toolbar's layout button
switches to a vertical splitter (table on the left). The theme button toggles
ImGui's dark/light styles.

Toolbar buttons: **Add movie**, **Settings**, **About**, **Layout**, **Theme**,
and **Check for updates**.

## Table panel

- A **title quick-filter** box filters as you type (`title ~ …`).
- **Filters…** opens a modal to build the full query: a list of `field op value`
  filters (the M4 grammar), an optional **Where** boolean expression
  (`F1 AND (F2 OR F3)`; blank = AND all), and a **Sort** spec (`year:desc,title`).
  See [querying.md](querying.md).
- The table renders the result set with a **clipper** (only visible rows are
  built, so thousands of films are cheap). Rows highlight on hover; click selects
  a film for the detail panel. Click column headers to **sort** (multi-sort with
  the modifier key). The header's right-click menu **chooses which columns** are
  shown — default columns are year, title, last seen, length, director, genres,
  FA and IMDb rating.

## Detail panel

Shows the selected film: cover art, title/year/runtime, director and genre
**chips**, and tabs for overview (synopsis, ratings, financials, topics/groups),
cast & crew, your data, and the video file (resolution/framerate/bitrate + audio
and subtitle tracks).

**Clickable chips apply a filter**: the year, each genre, and each director
narrow the table to matching films.

Buttons: **Update** (re-fetch the sources on a background thread, then a modal
lists what changed before you apply it — your own data is preserved), **Edit**
(a modal to change any field), **Mark seen today** (bumps watch count and sets
today's date), **Remove** (confirmation-gated), and **Play** (opens the video
file in the OS default player).

## Add movie

The **Add** modal takes a search query and checkboxes for which sources to search
(IMDb, FilmAffinity) and whether to also fetch financials and cover art. Results
appear in **per-source tabs** (title / year / cast). Pick the right entry in each
tab and **Add selected** fetches and merges them (per the IMDb-wins policy),
pulling financials from BoxOfficeMojo and the cover when requested.

## Updates

**Check for updates** runs the check on a background thread; if a newer release
exists the button becomes **Update available!** and opens a dialog with the
version and notes. **Update now** downloads and verifies in the background, then
hands off to the installer and relaunches — the app closes itself so the swap can
complete. A throttled auto-check also runs on launch (cadence in **Settings**,
default daily). The **About** dialog shows the version and build date; the first
launch after a successful update shows the release notes (or an error) once. The
full mechanism is in [updating.md](updating.md).

## Notes

- Network operations (search, fetch, update) run on a background thread; the UI
  stays responsive and shows a "working" state.
- Dear ImGui and stb_image are vendored under `third_party/` (each under its own
  license). GLFW is a vcpkg dependency; OpenGL is the system library.
- This is a first-draft GUI; layout/theme are per-session and further polish is
  expected.
