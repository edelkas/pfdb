# emdb.dat specification

This file documents the structure of the `emdb.dat` file used by [EMDB](https://www.emdb.eu/) to store its movie database.

It contains movie metadata extracted from sources like [IMDb](https://www.imdb.com/), [TMDB](https://www.themoviedb.org/), [TVDB](https://www.thetvdb.com/) or [RottenTomatoes](https://www.rottentomatoes.com/), as well as user-specific data and file metadata (when the video file has been provided).

## Table of contents

- [Table of contents](#table-of-contents)
- [General aspects](#general-aspects)
- [Version section](#version-section)
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

## Version section

This metadata section only indicates the `emdb.dat` file version, so EMDB knows how to parse it. In the old INI format it had no content, the version was embedded in the section name, e.g. `[V51]`. In the new JSON format, it's keyed by `version` and its value is an integer (as a string, as mentioned).

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
- Collection ID in TMDb (-1 if none)

### Year group

Second line in the INI format, keyed `year` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 17 fields:

- Year of release
- Comma-separated list of director indexes (see the [Directors](#directors) section)
- Length in minutes
- Country name or code (e.g. "USA" or "#US")
- Integer, each digit in base 10 contains an enum, from low to high:
  * ?
  * ?
  * ?
  * ?
  * Bit depth (0 = Unknown, 1 = 8 bits, 2 = 10 bits, 3 = 12 bits)
- ?
- [Resolution](#resolution) information, as a string or an enum
- Path to video file (empty if none)
- Film [edition](#editions) / version as an integer
- An integer with two components: A bitmask with personal information in the first byte, and the watch count afterwards:
  * The 8-bit personal bitmask encodes the following properties, from low to high:
    + ?
    + The movie is in the wish list
    + The user owns the movie
    + The movie shows in the short list
    + The information is locked to prevent changes
    + The movie is favourited
    + ?
    + ?
  * Amount of times the film has been watched. To extract it, the field must be right-shifted by 8 bits.
- Number of disks (default: 1)
- Integer, each digit encoding a different thing:
  * Units: Color information (0 = Not specified, 1 = Color, 2 = Black & White, 3 = B&W / Color)
  * Tens: ?
  * Hundreds: Frame rate (0 = Not specified, 1 = 23.976, 2 = 24, 3 = 25, 4 = 29.97, 5 = 30, 6 = 60)
- ?
- ?
- ?
- Comma-separated list of writer indexes (see the [Writers](#writers) section) (added in v51, 2016)
- Comma-separated list of composer indexes (see the [Composers](#composers) section) (added in v53, 2017)

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

### Genres group

Third line in the INI format, keyed `genres` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 11 fields:

- Genre list as a string (one-character code each, see [Genre list](#genre-list))
- Film ID in IMDb (followed by another number - 0 by default - separated by |)
- Date the film was added to the db (YYYMMDD)
- Rating
- ? 
- ? (default: empty)
- Price in cents
- Trailer URL (default: empty)
- Date the film was watched (YYYMMDD)
- URL to RottenTomatoes page, path only (empty if none) (added around v60, 2020)
- Film ID in TVDb (empty if none) (added around v65, 2021)

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

### Audio group

Fourth line in the INI format, keyed `audio` in the JSON format. The pre-v52 field separator of this group was `;`. It currently has 4 fields:

- ? (added in v52, 2017)
- ?
- ?
- Awards encoded as a string, see specification below

Awards are encoded as a string by concatenating one component per ceremony. Each component starts with a character identifying it, for example, `O` for the Oscars, `G` for the Golden Globes, and `B` for the BAFTA awards. Then comes a 2-digit number with the amount of wins. Optionally, it's followed by a 2-digit number with the amount of nominations (excluding wins).

Currently, nominations are only included for the Oscars. For example, the film Ben-Hur has the award string `O1101G04B01`, meaning 11 Oscar wins plus one additional nomination, 4 Golden Globe wins and 1 BAFTA award win.

### Cast group

Fifth line in the INI format, keyed `cast` in the JSON format. It has only two fields:

- Comma-separated list of actor indexes (see the [Actors](#actors) section)
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

- ?
- ? (default: 0)
- ? (default: 0)
- ? (default: 0)
- Feature bitmask, dumped as a hex number. Each bit meaning, from low to high:
  * Menu
  * Extras
  * ?
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
- [Language list](#languages) (one character each, see below)
- ? (empty by default)
- IMDb vote count
- File size in MB
- [RottenTomatoes stats](#rottentomatoes-stats) (added around v60, 2020)
- ? (added around v58, 2019)

#### Languages

Languages are stored as 1-character codes concatenated into a string:

| Language | Code |
| --- | --- |
| Arabic | a |
| Bengali | m |
| Bulgarian | q |
| Cantonese | 2 |
| Catalan | 3 |
| English | E |
| French | F |
| German | G |
| Italian | I |
| Japanese | J |
| Russian | R |
| Silent movie | & |
| Spanish | S |

#### RottenTomatoes stats

RottenTomatoes stats are currently encoded as an 8-character string:

| Characters | Value |
| --- | --- |
| 0-1 | Popcornmeter score |
| 2 | Verified hot (0 = No, 1 = Yes) |
| 3-4 | Tomatometer score |
| 5 | Certified fresh (0 = No, 1 = Yes) |
| 6-7 | ? |

They may also appear encoded as a 9-character string, for backwards compatibility:

| Characters | Value |
| --- | --- |
| 0-2 | ? (999 by default) |
| 3 | Certified fresh (0 = No, 1 = Yes) |
| 4-5 | Tomatometer score |
| 6 | Verified hot (0 = No, 1 = Yes) |
| 7-8 | Popcornmeter score |

### Custom fields group

Tenth line in the INI format, and it's keyed `custom-fields` in the JSON format. The pre-v52 field separator of this group was `|`. It serves the purpose of adding custom user fields (see also [Comments group](#comments-group)). It has 2 potentially empty fields. Will not appear in the JSON at all if both are empty.

- Custom field 1
- Custom field 2

### Tagline group

Added in v79 (2024), so it doesn't appear in the INI format, and it's keyed `tagline` in the JSON format. It has 3 fields, each empty if not available:

- Tag line of the movie
- Comma-separated list of tags indexes (see the [Tags](#tags) section)
- Web link / socials URL

## Actors section

This section stores the list of actors present in the database movies. Their indexes in this list are the ones used in each movie's cast (see [Cast group](#cast-group)), so it's vital to maintain its order in order to recover the mapping.

In the JSON format, it's an array where each entry is an actor, with only 2 keys: the name and the IMDb ID. If the data has been fetched from TMDB instead, the ID appears prefixed with `tmdb-`.

In the INI format, actors are separated by new lines, and each line contains the name and the ID separated by `|`.

## Directors section

This section stores the list of directors of the database movies. Again, their indexes in this list are used in each movie's metadata (see [Year group](#year-group)), so it's necessary to maintain it.

In the JSON format, it's an array where each entry is an actor, and it has a single key: the name. In the INI format, it's simply the list of names separated by new lines.

## Writers section

This section stores the list of writers of the database movies. Its formatting is identical to the [Directors](#directors) section, both in the new JSON format and the old INI format. This section was added in v51 (2016).

## Composers section

This section stores the list of soundtrack composers of the database movies. Its formatting is identical to the [Directors](#directors) section, both in the new JSON format and the old INI format. This section was added in v53 (2017). Note however that it was named `SoundtrackWriters` in the old INI format.

## Tags section

This section stores the list of available tags, i.e. topics (e.g. "Buddy Comedy" or "Cyberpunk"). It was probably added around v70 circa 2022, so it's only available in the JSON format.

It's an array where each entry is a tag, and each tag has 3 string fields: the name, the ID (its position in the array), and the color of the badge. The color is encoded as an integer in RGB format (little-endian).

## Collections section

This section stores TMDb movie collections. It was probably added around v70 circa 2022, so it's only available in the JSON format.

It's an array where each entry is a collection, and each tag has 3 string fields: the name, the sort-string (the name used for sorting), and the ID in TMDb.