#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "pfdb/types.hpp"

namespace pfdb {

/// Metadata about the local video file backing a film, when the user has one.
/// All fields are optional because a film may have no file, or only partial
/// information may be known.
struct VideoFileInfo {
    /// Absolute or user-relative path to the video file.
    std::string path;
    std::optional<std::int64_t> size_bytes;
    std::optional<int> duration_seconds;
    std::optional<int> width;
    std::optional<int> height;
    /// Container/codec description, e.g. "mkv/h265".
    std::string codec;

    friend bool operator==(const VideoFileInfo&, const VideoFileInfo&) = default;
};

/// User-specific data that is not part of any external source: how *this*
/// user relates to the film.
struct UserData {
    /// Date the user watched the film ("YYYY-MM-DD"), if recorded.
    std::optional<IsoDate> date_watched;
    /// The user's own score, on a 0-10 scale by convention.
    std::optional<double> personal_rating;
    /// Free-form notes.
    std::string notes;
    /// Whether the user flagged this film as a favourite.
    bool favorite = false;
    /// Number of times the user has watched the film (0 = unwatched).
    int watch_count = 0;
    /// Whether the user owns a copy of the film.
    bool owned = false;
    /// Whether the film is on the user's wish list.
    bool wishlist = false;

    friend bool operator==(const UserData&, const UserData&) = default;
};

}  // namespace pfdb
