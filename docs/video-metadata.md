# Local video-file metadata

PFDB can index the technical metadata of the local video file backing a film,
using the [MediaInfo](https://mediaarea.net/en/MediaInfo) library (`libmediainfo`,
via vcpkg) rather than reinventing container parsing.

```sh
pfdb scan <id> [--file <path>]
```

`scan` probes the file and stores its metadata on the film. With `--file` it uses
that path; otherwise it re-probes the film's already-stored `video.path` (e.g. one
imported from EMDB). The probe is wrapped in `src/media/mediainfo.cpp`
(`pfdb::media::probe`), which links the MediaInfo DLL's C API through
`MediaInfoDLL_Static.h`.

## What it captures

- **General / video:** file size, duration, width, height, frame rate, video
  bitrate, video codec.
- **Per audio track:** name, language, size, codec, bitrate, channels, sample rate.
- **Per subtitle track:** name, language, size, format.

These populate `VideoFileInfo` (see [data-model.md](data-model.md)); the audio and
subtitle tracks live in their own tables. `pfdb list --json` shows the full
structure under each film's `video`.

## Playback

```sh
pfdb play <id>
```

opens the film's stored `video.path` in the operating system's default
application (ShellExecute on Windows; `xdg-open` / `open` elsewhere). It errors if
the film has no video path.

## Testing

MediaInfo needs real files, so the deterministic tests cover the pure value
normalization helpers (`parse_int_loose` / `parse_double_loose`, which turn
MediaInfo's `"1 920"` / `"23.976"` strings into numbers). The end-to-end probe is
exercised manually (e.g. `pfdb scan <id> --file some.mkv`).
