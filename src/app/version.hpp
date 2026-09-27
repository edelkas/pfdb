#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pfdb::app {

/// A semantic version (https://semver.org), reduced to what PFDB needs:
/// `major.minor.patch` plus an optional pre-release tag. Build metadata
/// (`+...`) is parsed but ignored for ordering, per the spec.
struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string pre;  // pre-release identifier, e.g. "rc.1" (empty = release)

    /// A release (empty pre-release) sorts *after* any pre-release of the same
    /// core version, exactly as SemVer precedence requires.
    friend bool operator==(const SemVer& a, const SemVer& b);
    friend bool operator<(const SemVer& a, const SemVer& b);
    friend bool operator!=(const SemVer& a, const SemVer& b) { return !(a == b); }
    friend bool operator>(const SemVer& a, const SemVer& b) { return b < a; }
    friend bool operator<=(const SemVer& a, const SemVer& b) { return !(b < a); }
    friend bool operator>=(const SemVer& a, const SemVer& b) { return !(a < b); }

    /// Render back to a string ("1.2.3" or "1.2.3-rc.1"). Never includes a
    /// leading 'v'.
    std::string str() const;
};

/// Parse a version string, tolerating an optional leading 'v'/'V' and build
/// metadata after '+'. Returns nullopt if the core `major.minor.patch` is
/// missing or malformed.
std::optional<SemVer> parse_semver(std::string_view text);

/// The running build's version, from the PFDB_VERSION macro injected by CMake.
SemVer current_version();

}  // namespace pfdb::app
