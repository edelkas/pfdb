#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Common vocabulary types shared across the PFDB domain model.
namespace pfdb {

/// Database row identifier. 0 means "not yet persisted / invalid".
using Id = std::int64_t;

inline constexpr Id kInvalidId = 0;

/// A point in time, stored throughout PFDB as Unix seconds (UTC).
/// We use a plain integer rather than std::chrono types so persistence,
/// JSON, and CLI formatting all agree on one unambiguous representation.
using UnixSeconds = std::int64_t;

/// A calendar date without a time component, encoded as an ISO-8601 string
/// ("YYYY-MM-DD"). Kept as a string because that is exactly how the user
/// enters "date watched" and how we round-trip it to SQLite and JSON.
using IsoDate = std::string;

/// Optional identifier used for parent/child links that may be unset.
using OptionalId = std::optional<Id>;

}  // namespace pfdb
