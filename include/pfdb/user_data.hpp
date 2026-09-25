#pragma once

#include <optional>
#include <string>

#include "pfdb/types.hpp"
#include "pfdb/video_file.hpp"  // VideoFileInfo (kept here for source compatibility)

namespace pfdb {

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
