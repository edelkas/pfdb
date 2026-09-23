# emdb.dat specification

This file documents the structure of the `emdb.dat` file used by [EMDB](https://www.emdb.eu/) to store its movie database.

It contains movie metadata extracted from sources like [IMDb](https://www.imdb.com/), [TMDB](https://www.themoviedb.org/), [TVDB](https://www.thetvdb.com/) or [RottenTomatoes](https://www.rottentomatoes.com/), as well as user-specific data and file metadata (when the video file has been provided).

## Table of contents

- [Table of contents](#table-of-contents)
- [General aspects](#general-aspects)
   * [Text escaping](#text-escaping)
- [Version section](#version-section)
- [Custom property sections](#custom-property-sections)
- [Movies section](#movies-section)
   * [Title group](#title-group)
   * [Year group](#year-group)
   * [Genres group](#genres-group)
   * [Audio group](#audio-group)
   * [Cast group](#cast-group)
   * [Plot group](#plot-group)
   * [Comments group](#comments-group)
   * [Loaned group](#loaned-group)
   * [Rating group](#rating-group)
   * [Custom fields group](#custom-fields-group)
   * [Tagline group](#tagline-group)
- [Actors section](#actors-section)
- [Directors section](#directors-section)
- [Writers section](#writers-section)
- [Composers section](#composers-section)
- [Tags section](#tags-section)
- [Collections section](#collections-section)

## General aspects

The file is versioned independently of EMDB itself, and the current version as of September 2026 is v80. All details in this documentation refer to this version, although many apply for significantly older ones too, and some of the changes will be indicated.

The file's encoding has been UTF-16 LE w/ BOM since as far back as my records go (v44, 2014), and it's used a JSON format since at least v58 (2019). The previous versions used an INI-style format instead.

The file is divided into sections. In the current JSON format, each section is a top-level key. In the old INI-style format, each section was introduced by a line with the name in brackets, as usual, and the end of the file was marked by a `[TheEnd]` section.

In the JSON format, all numerical values appear as strings, and thus need to be casted during parsing.

The top-level keys of the JSON format, in the order EMDB writes them, are: `version`, `custom-genres`, `custom-languages`, `custom-services`, `custom-versions`, `custom-theaters`, `collections`, `tags`, `movies`, `actors`, `directors`, `writers` and `composers`. The executable also knows a `groups` key (the successor of the INI-style `[Groups]` section), but it isn't written by current versions.

### Text escaping

Although the file looks like JSON, it isn't strictly valid JSON, so a standard parser needs some leniency:

- Backslashes are **not** escaped (e.g. file paths appear as `"I:\Peliculas\Mad Max.avi"`), so they must be doubled before parsing.
- Field values contain raw control characters (the `0x1E` Record Separator), which strict parsers reject.
- Instead of JSON escapes, EMDB replaces a few characters with tags inside field values (these tags also occur in the codes of the [Audio group](#audio-group)):

| Tag | Character |
| --- | --- |
| `<DQ>` | `"` |
| `<SC>` | `[` |
| `<SCB>` | `]` |
| `<CB>` | `{` |
| `<CCB>` | `}` |

## Version section

This metadata section only indicates the `emdb.dat` file version, so EMDB knows how to parse it. In the old INI format it had no content, the version was embedded in the section name, e.g. `[V51]`. In the new JSON format, it's keyed by `version` and its value is an integer (as a string, as mentioned).

## Custom property sections

EMDB lets the user define up to 10 custom values for several enumerations, from Options → Custom Properties. They are stored in the top-level objects `custom-genres`, `custom-languages`, `custom-services`, `custom-versions` and `custom-theaters`, each mapping a 2-digit slot index (`"00"` to `"09"`) to the user-given name, e.g. `{"00": "Kaiju", "01": "Giallo"}`. They're empty objects (`{}`) if unused. In the INI format, the equivalent sections were `[Custom Genres]` and `[Custom Languages]`.

Movies reference them with the following codes:

| Section | Custom slot 1 to 10 is encoded as | Used in |
| --- | --- | --- |
| `custom-genres` | Characters `1` `2` `3` `4` `5` `6` `7` `8` `9` `0` | [Genre list](#genre-list) |
| `custom-languages` | Characters `!` `@` `$` `#` `5` `6` `^` `9` `*` `(` | [Languages](#languages) |
| `custom-services` | Values 2 to 6 (slots 1-5) and 25 to 29 (slots 6-10) | [Streaming services](#streaming-services) |
| `custom-versions` | Values 17 to 26 | [Editions](#editions) |
| `custom-theaters` | Values 13 to 22 | [Movie theaters](#movie-theaters) |

The labels of the two custom fields (see [Custom fields group](#custom-fields-group)) are not stored here, but in `emdb.cfg` (`CustomField1` and `CustomField2`).

## Movies section

This section contains most of the films' metadata and userdata. In the current JSON format, it's an array where each entry is a movie object. In the old INI-style format, movies were delimited by the Group Separator character - ASCII `0x1D` - since v52 (2017), and before that, they were a fixed amount of lines each.

For each movie, its fields are organized into groups, and each group is formatted as a string that needs to be parsed to extract the individual fields. In the JSON format, each group is simply a different key whose value is a string. In the INI format, each group is a different line, so the ordering matters. Unused groups may not appear in the JSON format, whereas in the INI format they would show as an empty line, or a line with only the separator, perhaps with the default values.

Fields within each group are delimited by the Record Separator character - ASCII `0x1E` - since v52, and before that, each group had a different printable field separator (usually `|`, `;` or `-`).

The available field groups, in order, are the following:

- [Title group](#title-group)
- [Year group](#year-group)
- [Genres group](#genres-group)
- [Audio group](#audio-group)
- [Cast group](#cast-group)
- [Plot group](#plot-group)
- [Comments group](#comments-group)
- [Loaned group](#loaned-group)
- [Rating group](#rating-group)
- [Custom fields](#custom-fields-group)
- [Tagline group](#tagline-group)

Note the names listed here are the key names in the JSON format, they weren't named in the INI format. The name usually refer to the first field in the group, but each group actually contains a wide range of different fields. Unknown fields are denoted with a `?`.

### Title group

First line in the INI format, keyed `title` in the JSON format. The pre-v52 field separator of this group was `|`. It currently has 4 fields, but was known to have at least one more previously:

- Title
- Also known as (empty if none)
- Studio company
- Unknown (removed at some point circa v70 / 2022).
- Collection ID (-1 if none), matching the `id` of an entry in the [Collections](#collections-section) section. It's the TMDb ID for imported collections.

### Year group

Second line in the INI format, keyed `year` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 17 fields:

- Year of release
- Comma-separated list of director indexes (see the [Directors](#directors-section) section)
- Length in minutes
- Country name or code (e.g. "USA" or "#US")
- Integer packing three enums in base 10, computed as `codec + 100 × container + 10000 × bit_depth`:
  * Units and tens: [Video codec](#video-codecs)
  * Hundreds and thousands: [Container](#containers)
  * Ten thousands: Bit depth (0 = Unknown, 1 = 8 bits, 2 = 10 bits, 3 = 12 bits)
- [Aspect ratio](#aspect-ratios) as an enum
- [Resolution](#resolution) information, as a string or an enum
- Location, i.e. path to the video file (empty if none)
- Film [edition](#editions) / version as an integer
- An integer with two components: A bitmask with personal information in the first byte, and the play count afterwards:
  * The 8-bit personal bitmask encodes the following properties, from low to high:
    + Seen (legacy). Before play counts existed (up to circa 2019), this was the only "seen" marker. Current versions ignore it and clear it when the movie is saved again, so it only survives in movies not edited since then (always along with a play count ≥ 1).
    + The movie is in the wish list
    + The user owns the movie
    + The movie shows in the short list. EMDB appears to clear it when restarted, so it's effectively session-only.
    + The information is locked to prevent changes (while set, EMDB won't save any other changes to the movie)
    + The movie is favourited
    + Unused
    + Unused
  * Play count, i.e. amount of times the film has been watched. To extract it, the field must be right-shifted by 8 bits. "Mark as seen" in the context menu increments it and sets the date seen (see [Genres group](#genres-group)); a movie counts as seen when this is ≥ 1.
- Number of disks (default: 1)
- Integer, each digit encoding a different thing:
  * Units: Color information (0 = Not specified, 1 = Color, 2 = Black & White, 3 = B&W / Color)
  * Tens: Unused (always 0)
  * Hundreds: Frame rate (0 = Not specified, 1 = 23.976, 2 = 24, 3 = 25, 4 = 29.97, 5 = 30, 6 = 60)
- Video system (0 = Not specified, 1 = PAL, 2 = NTSC, 3 = SECAM)
- [Region](#regions) as an enum
- Unique movie ID. It's sequential and never reused, and it's also used to name the movie's image files (e.g. `Posters\000116.jpg`, `Covers\000001.jpg`) and its TV series folder (e.g. `TVSeries\001560\`).
- Comma-separated list of writer indexes (see the [Writers](#writers-section) section) (added in v51, 2016)
- Comma-separated list of composer indexes (see the [Composers](#composers-section) section) (added in v53, 2017)

#### Resolution

The resolution information may be stored directly as a string if supplied manually, or as an enum if one of the default values is chosen (in which case the value will appear prefixed with `@`). The enum takes the following values:

| Value | Meaning |
| --- | --- |
| 0 | 720x576 (PAL DVD) |
| 1 | 720x480 (NTSC DVD) |
| 2 | 1280x720 (BluRay) |
| 3 | 1920x1080 (BluRay) |
| 4 | 1080p |
| 5 | 1080i |
| 6 | 720p |
| 7 | 720i |
| 8 | 3840x2160 (4K UHD) |
| 9 | 2160p |

#### Editions

The movie editions / versions are encoded as an enum with the following values:

| Value | Meaning |
| --- | --- |
| -1 | Not set |
| 0 | Unrated |
| 1 | Extended |
| 2 | Director's cut |
| 3 | Uncut |
| 4 | Special edition |
| 5 | Collector's edition |
| 6 | Ultimate edition |
| 7 | Limited edition |
| 8 | Anniversary edition |
| 9 | Deluxe edition |
| 10 | Platinum edition |
| 11 | Theatrical version |
| 12 | Remastered |
| 13 | TV movie |
| 14 | Straight to video |
| 15 | IMAX |
| 16 | Criterion collection |
| 17-26 | [Custom versions](#custom-property-sections) 1 to 10 |

#### Video codecs

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | MPEG-2 (DVD) |
| 2 | XviD |
| 3 | DivX |
| 4 | WMV |
| 5 | MPEG-1 (VCD/SVCD) |
| 6 | H.264 |
| 7 | RealMedia |
| 8 | MPEG-2 Part 2 |
| 9 | H.264/MPEG-4 AVC |
| 10 | SMPTE VC-1 |
| 11 | MPEG-4 |
| 12 | Flash |
| 13 | H.265 HEVC |
| 14 | MPEG-4 Part 2 |
| 15 | VP8 |
| 16 | VP9 |
| 17 | AV1 |

#### Containers

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | Matroska (MKV) |
| 2 | MPEG program stream |
| 3 | AVI (Microsoft) |
| 4 | Quicktime (Apple) |
| 5 | MPEG-4 |
| 6 | Ogg |
| 7 | Flash Video (Adobe) |
| 8 | RealMedia (RM) |
| 9 | 3GP (Mobile phones) |
| 10 | VOB |
| 11 | MPEG-2 transport stream (MPEG-TS) |
| 12 | Windows Media |
| 13 | WebM |

#### Aspect ratios

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | Anamorphic Widescreen |
| 2 | 16:9 Widescreen |
| 3 | 4:3 |
| 4 | 2.35:1 |
| 5 | 1.85:1 |
| 6 | 1.78:1 |
| 7 | 2.40:1 |
| 8 | 2.20:1 |
| 9 | 1.66:1 |
| 10 | 2.55:1 |
| 11 | 5:4 |
| 12 | 2.39:1 |
| 13 | 1.37:1 (Academy ratio) |
| 14 | 2.76:1 |
| 15 | 2:1 |
| 16 | 1.19:1 |
| 17 | 1.75:1 |
| 18 | 16:10 |

#### Regions

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | DVD 1: United States, Canada |
| 2 | DVD 2: Europe, Middle East, South Africa |
| 3 | DVD 3: Southeast Asia, Hong Kong |
| 4 | DVD 4: South America, Mexico, Australia |
| 5 | DVD 5: Russia, Africa |
| 6 | DVD 6: China |
| 7 | DVD 7: Reserved |
| 8 | DVD 8: Aircraft, cruise ships |
| 9 | DVD 0: Region free |
| 10 | Bluray A/1: North America |
| 11 | Bluray B/2: Europe, Africa, Middle East, Australia |
| 12 | Bluray C/3: China, Russia |

### Genres group

Third line in the INI format, keyed `genres` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 11 fields:

- Genre list as a string (one-character code each, see [Genre list](#genre-list))
- Film ID in IMDb, followed by `|` and the film's position in the IMDb Top 250 (0 if not in it), e.g. `7286456|50`. The pre-v52 format had the ID only.
- Date the film was added to the db (YYYYMMDD)
- Certification / age rating, as free text (e.g. "R", "PG-13", "18", "T")
- [TV series information](#tv-series-information), `@`-separated
- ? (always empty)
- Value (price paid) in cents
- Trailer URL (default: empty)
- Date the film was last seen / played (YYYYMMDD). Shown as "Last played" in the edit dialog, and set to the current date by "Mark as seen".
- URL to RottenTomatoes page, path only (empty if none) (added around v60, 2020)
- Film ID in TVDb (empty if none) (added around v65, 2021)

#### TV series information

This field holds up to three `@`-separated components, e.g. `1@0@0`. They're only meaningful for entries flagged as TV series (`#` genre); the rest of the series data (seasons, episodes, etc.) is kept in `TVSeries\<movie ID>\<movie ID>.dat`.

- Flags as characters. `!` means "Complete Series". Most movies have a legacy `1` here, which current versions replace when the flags are edited.
- Series status: -1 or 0 = Unknown, 1 = Ended, 2 = Continuing, 3 = Cancelled, 4 = Renewed
- Unknown, 0 by default. Added in a later version: older entries only have two components, and EMDB appends `@0` when it saves them again.

#### Genre list

Genres are taken from IMDb, and abbreviated with the following one-character codes:

| Genre | Code |
| --- | --- |
| Action | A |
| Adult | X |
| Adventure | V |
| Animation | C |
| Biography | @ |
| Comedy | K |
| Crime | ! |
| Documentary | U |
| Drama | D |
| Family | I |
| Fantasy | F |
| Film-noir | f |
| Game show | g |
| History | G |
| Horror | H |
| Music | M |
| Musical | m |
| Mystery | Y |
| News | N |
| Reality TV | r |
| Romance | R |
| Sci-Fi | S |
| Short | s |
| Sport | P |
| Talk show | t |
| Thriller | T |
| War | O |
| Western | W |
| TV series | \# |
| [Custom genres](#custom-property-sections) 1 to 9 | 1 to 9 |
| [Custom genre](#custom-property-sections) 10 | 0 |

### Audio group

Fourth line in the INI format, keyed `audio` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 4 fields:

- Audio stream formats, one [audio format](#audio-formats) code per stream (empty if none)
- Audio stream languages, one [language](#languages) code per stream, in the same order as the formats (empty if none)
- Custom number, a free user field (Additional Info → "Custom Number") (added in v52, 2017)
- Awards encoded as a string, see specification below

For example, the pair `de` / `SD` means two audio streams: DTS 5.1 in Spanish and PCM in Dutch.

#### Audio formats

The format codes follow the order of the `[SOUND]` section of EMDB's language files (IDs 2199 onward): lowercase letters first, then punctuation, then uppercase letters. Some codes are stored escaped (see [Text escaping](#text-escaping)).

| Format | Code |
| --- | --- |
| Unknown | `a` |
| MPEG2 Layer 3 (MP3) | `b` |
| Dolby Digital 5.1 | `c` |
| DTS 5.1 | `d` |
| PCM | `e` |
| Dolby Digital 2.0 | `f` |
| Dolby Digital Plus 7.1 | `g` |
| DTS-HD Master Audio | `h` |
| Dolby TrueHD | `i` |
| Mono | `j` |
| Dolby 4.1 | `k` |
| Dolby Digital EX Matrix | `l` |
| Dolby Digital EX 6.1 | `m` |
| Stereo | `n` |
| Dolby Surround | `o` |
| DTS-HD High Resolution Audio | `p` |
| PCM (duplicate entry, not selectable) | `q` (probably) |
| AAC 2.0 | `r` |
| AAC 5.1 | `s` |
| DTS-HD Master Audio 2.0 | `t` |
| DTS-HD Master Audio 5.1 | `u` |
| DTS-HD Master Audio 7.1 | `v` |
| Dolby Atmos | `w` |
| Dolby Digital 1.0 | `x` |
| DTS-HD Master Audio 1.0 | `y` |
| Windows Media Audio (WMA) | `z` |
| DTS:X (older of two entries) | `<CB>` (`{`) |
| DTS-HD Master Audio 6.1 | `<CCB>` (`}`) |
| AAC+ 2.0 | `<SC>` (`[`) |
| AAC+ 5.1 | `<SCB>` (`]`) |
| AAC Mono | `(` |
| AAC 7.1 | `)` |
| DTS-ES 5.1 | `A` |
| DTS-ES 6.1 | `B` |
| Auro-3D 9.1 | `C` |
| Auro-3D 10.1 | `D` |
| Auro-3D 11.1 | `E` |
| Dolby Digital Plus 2.0 | `F` |
| Dolby Digital Plus 5.1 | `G` |
| Ogg Vorbis 2.0 | `H` |
| FLAC | `I` |
| DTS 96/24 | `J` |
| AAC Low Complexity | `K` |
| DTS:X (newer of two entries) | `L` |

#### Awards

Awards are encoded as a string by concatenating one component per ceremony. Each component starts with a character identifying it, for example, `O` for the Oscars, `G` for the Golden Globes, and `B` for the BAFTA awards. Then comes a 2-digit number with the amount of wins. Optionally, it's followed by a 2-digit number with the amount of nominations (excluding wins).

Currently, nominations are only included for the Oscars. For example, the film Ben-Hur has the award string `O1101G04B01`, meaning 11 Oscar wins plus one additional nomination, 4 Golden Globe wins and 1 BAFTA award win.

### Cast group

Fifth line in the INI format, keyed `cast` in the JSON format. It has only two fields:

- Comma-separated list of actor indexes (see the [Actors](#actors-section) section)
- Character names, in the same order, `|`-separated.

The pre-v52 field separator of this group was `|`, which can cause confusion since character names are also separated by vertical bars. However, since the field count is known (only 2), only the first vertical bar is actually a field separator.

### Plot group

Sixth line in the INI format, keyed `plot` in the JSON format. Its only content is the actual synopsis of the movie. Will not appear in the JSON at all if empty.

### Comments group

Seventh line in the INI format, keyed `comments` in the JSON format. Its only content is the user's personal comments about the movie (see also [Custom fields group](#custom-fields-group)). Will not appear in the JSON at all if empty.

### Loaned group

Eighth line in the INI format, keyed `loaned` in the JSON format. It contains the user's loan history of this movie, in reverse chronological order. Will not appear in the JSON at all if the movie has never been loaned.

Loans are separated by `|`, and each one is formatted as `loanee_name/date_loaned\date_returned`, where each date follows the format `YYYYMMDD`, and the last part `\date_returned` is optional for the current loan (meaning the movie has not yet been returned).

### Rating group

Ninth line in the INI format, keyed `rating` in the JSON format. The pre-v52 field separator of this group was `-`. It currently has 11 fields:

All numbers in the first five fields are written in hexadecimal, zero-padded to at least 2 digits.

- IMDb rating × 10, e.g. `50` = 8.0
- User rating × 10 (default: `00`, i.e. not rated), e.g. `19` = 2.5
- `medium + 1000 × case`, i.e. the [Medium](#media) in the low decimal digits and the [Case](#cases) in the thousands. E.g. `5227` = 21031 = 4K UHD Steelbook + Video 2000 tape.
- `source + 100 × service + 100000 × theater`, i.e. the [Source](#sources), the [Streaming service](#streaming-services) and the [Movie theater](#movie-theaters) packed in decimal. E.g. `391` = 913 = Amazon Prime (9) + Streaming (13). The service and theater are only shown when the source is Streaming or Movie Theater, respectively, but they're kept when the source changes.
- Feature bitmask. Each bit meaning, from low to high:
  * Menu
  * Extras
  * Subtitles (set automatically when there are subtitle languages)
  * 3-D
  * 3-D Side By Side (SDS) (implies 3-D)
  * 3-D + 2-D (implies 3-D)
  * D-Box
  * 3-D Over/Under (OU) (implies 3-D)
  * HDR10
  * HDR10+
  * Dolby Vision
  * HLG
  * Trailers (implies Extras)
  * Director's commentary (implies Extras)
  * Alternative endings (implies Extras)
  * Cut scenes (implies Extras)
  * Behind the scenes (implies Extras)
  * HDR10 and Dolby Vision
  * HDR10+ and Dolby Vision
- Spoken languages, see [Languages](#languages) (one character each)
- Subtitle languages, see [Languages](#languages) (one character each, empty by default)
- IMDb vote count
- File size in MB
- [RottenTomatoes and Metacritic stats](#rottentomatoes-and-metacritic-stats) (added around v60, 2020)
- EAN / UPC barcode (empty by default) (added around v58, 2019)

#### Languages

Languages are stored as 1-character codes concatenated into a string. The same codes are used for spoken languages, subtitles and audio streams:

| Language | Code |
| --- | --- |
| Arabic | `a` |
| Bengali | `m` |
| Bulgarian | `q` |
| Cantonese | `2` |
| Catalan | `3` |
| Chinese | `)` |
| Croatian | `c` |
| Czech | `C` |
| Danish | `d` |
| Dutch | `D` |
| English | `E` |
| Estonian | `Q` |
| Finnish | `f` |
| Flemish | `l` |
| French | `F` |
| Georgian | `x` |
| German | `G` |
| Greek | `g` |
| Hebrew | `e` |
| Hindi | `h` |
| Hungarian | `H` |
| Icelandic | `j` |
| Indonesian | `O` |
| Italian | `I` |
| Japanese | `J` |
| Korean | `K` |
| Latin | `7` |
| Latvian | `k` |
| Lithuanian | `U` |
| Luxembourgish | `8` |
| Macedonian | `X` |
| Mandarin | `M` |
| Maya | `%` |
| Mongolian | `V` |
| Norwegian | `N` |
| Persian | `A` |
| Polish | `p` |
| Portuguese | `P` |
| Romanian | `r` |
| Russian | `R` |
| Serbian | `4` |
| Silent movie | `&` |
| Slovak | `s` |
| Slovenian | `L` |
| Spanish | `S` |
| Swedish | `1` |
| Tagalog (Filipino) | `n` |
| Tamil | `W` |
| Thai | `T` |
| Turkish | `t` |
| Ukrainian | `y` |
| Unknown | `?` |
| Urdu | `u` |
| Vietnamese | `z` |
| Yiddish | `Y` |
| [Custom languages](#custom-property-sections) 1 to 10 | `!` `@` `$` `#` `5` `6` `^` `9` `*` `(` |

EMDB's language files also list Albanian, Portuguese (Brazil), Simplified Chinese and Traditional Chinese, but they can't be selected in the UI, so their codes (if any) are unknown.

#### Media

The medium enum follows the `[MEDIA]` section of the language files (value = ID − 2300):

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | Original DVD |
| 2 | DVD R/RW |
| 3 | SVCD |
| 4 | VCD |
| 5 | VHS tape |
| 6 | Hi8 tape |
| 7 | Betamax tape |
| 8 | Bluray |
| 9 | HD DVD |
| 10 | Digital Copy |
| 11 | LaserDisc |
| 12 | D-Theater (D-VHS) |
| 13 | CD-R/RW |
| 14 | DVD5 |
| 15 | DVD9 |
| 16 | BD25 |
| 17 | BD50 |
| 18 | Bluray + DVD |
| 19 | BD25 + DVD5 |
| 20 | Streaming |
| 21 | DVD + Digital Copy |
| 22 | Bluray + Digital Copy |
| 23 | Bluray + DVD + Digital Copy |
| 24 | Ultra HD Bluray |
| 25 | Media file |
| 26 | UHD + BD + Digital Copy |
| 27 | UHD + BD |
| 28 | CED Video Disc |
| 29 | UHD + Digital Copy |
| 30 | UMD (Universal Media Disc) |
| 31 | Video 2000 tape |

#### Cases

The case enum follows the `[CASES]` section of the language files (value = ID − 2350):

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | DVD Case |
| 2 | Slimline DVD Case |
| 3 | Bluray Case |
| 4 | Slimline Bluray Case |
| 5 | Bluray Steelbook |
| 6 | Cardboard Sleeve |
| 7 | Spindle |
| 8 | Media File |
| 9 | CD Jewel Case |
| 10 | Bluray Book |
| 11 | Digipak |
| 12 | Steelcase |
| 13 | DVD Steelbook |
| 14 | 4K UltraHD Case |
| 15 | DVD Snap Case |
| 16 | VHS Clamshell |
| 17 | VHS Sleeve |
| 18 | MediaBook |
| 19 | Plastic sleeve |
| 20 | Redbox |
| 21 | 4K UHD Steelbook |

#### Sources

The source enum follows the `[SOURCES]` section of the language files (value = ID − 2400):

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | DVD |
| 2 | DivX / XviD |
| 3 | TV |
| 4 | CAM |
| 5 | Telecine |
| 6 | Video tape |
| 7 | Digital TV |
| 8 | Satellite |
| 9 | Bluray Disc |
| 10 | HD DVD |
| 11 | LaserDisc |
| 12 | D-Theater (D-VHS) |
| 13 | Streaming |
| 14 | HDTV |
| 15 | Ultra HD Bluray |
| 16 | Movie Theater |

#### Streaming services

The service enum follows the `[SERVICES]` section of the language files (value = ID − 2550). The gaps in those IDs are the custom services:

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | Netflix |
| 2-6 | [Custom services](#custom-property-sections) 1 to 5 |
| 7 | Hulu |
| 8 | Max |
| 9 | Amazon Prime |
| 10 | Starz |
| 11 | Film1 |
| 12 | Videoland |
| 13 | Paramount+ |
| 14 | Sling TV |
| 15 | Sony Crackle |
| 16 | Direct TV |
| 17 | Apple TV+ |
| 18 | Disney+ |
| 19 | iTunes |
| 20 | Google Play |
| 21 | Youtube |
| 22 | VUDU |
| 23 | Movies Anywhere |
| 24 | NOW TV |
| 25-29 | [Custom services](#custom-property-sections) 6 to 10 |
| 30 | Peacock (from the language file; not selectable in v5.41) |
| 31 | ESPN+ (from the language file; not selectable in v5.41) |

#### Movie theaters

| Value | Meaning |
| --- | --- |
| 0 | Not specified |
| 1 | AMC |
| 2 | Cineworld |
| 3 | Cinemark |
| 4 | Cinépolis |
| 5 | CGV |
| 6 | Cinemex |
| 7 | Vue |
| 8 | Cineplex |
| 9 | Wanda |
| 10 | Pathé |
| 11 | Metropolis |
| 12 | Kinepolis |
| 13-22 | [Custom theaters](#custom-property-sections) 1 to 10 |

#### RottenTomatoes and Metacritic stats

These stats are currently encoded as an 8-character string:

| Characters | Value |
| --- | --- |
| 0-1 | Popcornmeter score |
| 2 | Verified hot (0 = No, 1 = Yes) |
| 3-4 | Tomatometer score |
| 5 | Certified fresh (0 = No, 1 = Yes) |
| 6-7 | Metascore (Metacritic) |

For example, Blade Runner 2049 has `88188081`: 88% Popcornmeter (verified hot), 88% Tomatometer, Metascore 81.

They may also appear encoded as a 9-character string, for backwards compatibility:

| Characters | Value |
| --- | --- |
| 0-2 | Always `999`, probably an unused Popcornmeter slot |
| 3 | Certified fresh (0 = No, 1 = Yes, 9 = no score) |
| 4-5 | Tomatometer score (`99` = no score) |
| 6 | Always 0 |
| 7-8 | Metascore (Metacritic), `00` if none |

For example, Joker has `999068059`: 68% Tomatometer, Metascore 59. The default `999999000` means neither score is available. Characters 3-5 can also be `100`, which seems to mean a 100% Tomatometer score (not certified), so that form is ambiguous.

### Custom fields group

Tenth line in the INI format, and it's keyed `custom-fields` in the JSON format. The pre-v52 field separator of this group was `|`. It serves the purpose of adding custom user fields (see also [Comments group](#comments-group)). It has 2 potentially empty fields. Will not appear in the JSON at all if both are empty.

- Custom field 1
- Custom field 2

### Tagline group

Added in v79 (2024), so it doesn't appear in the INI format, and it's keyed `tagline` in the JSON format. It has 3 fields, each empty if not available:

- Tag line of the movie
- Comma-separated list of tags indexes (see the [Tags](#tags-section) section)
- Web link / socials URL

## Actors section

This section stores the list of actors present in the database movies. Their indexes in this list are the ones used in each movie's cast (see [Cast group](#cast-group)), so it's vital to maintain its order in order to recover the mapping.

In the JSON format, it's an array where each entry is an actor, with only 2 keys: the name and the IMDb ID. If the data has been fetched from TMDB instead, the ID appears prefixed with `tmdb-`.

In the INI format, actors are separated by new lines, and each line contains the name and the ID separated by `|`.

## Directors section

This section stores the list of directors of the database movies. Again, their indexes in this list are used in each movie's metadata (see [Year group](#year-group)), so it's necessary to maintain it.

In the JSON format, it's an array where each entry is an actor, and it has a single key: the name. In the INI format, it's simply the list of names separated by new lines.

## Writers section

This section stores the list of writers of the database movies. Its formatting is identical to the [Directors](#directors-section) section, both in the new JSON format and the old INI format. This section was added in v51 (2016).

## Composers section

This section stores the list of soundtrack composers of the database movies. Its formatting is identical to the [Directors](#directors-section) section, both in the new JSON format and the old INI format. This section was added in v53 (2017). Note however that it was named `SoundtrackWriters` in the old INI format.

## Tags section

This section stores the list of available tags, i.e. topics (e.g. "Buddy Comedy" or "Cyberpunk"). It was probably added around v70 circa 2022, so it's only available in the JSON format.

It's an array where each entry is a tag, and each tag has 3 string fields: the name, the ID (its position in the array), and the color of the badge. The color is encoded as an integer in RGB format (little-endian).

## Collections section

This section stores TMDb movie collections. It was probably added around v70 circa 2022, so it's only available in the JSON format.

It's an array where each entry is a collection, and each collection has 3 string fields: the name, the sort-string (the name used for sorting), and the ID. Movies belong to a collection by referencing this ID (see [Title group](#title-group)).

For collections imported from TMDb, the ID is the TMDb collection ID. Collections created manually (Options → Collections) get placeholder IDs counting down from 999999 (999999, 999998, and so on), regardless of what's typed in the ID box.