#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace pfdb::query {

/// ASCII-only lowercase. Accented characters (á, é, …) are left unchanged, so
/// matching is case-insensitive for ASCII but accent-sensitive; this is a
/// documented limitation of the M4 query engine.
inline std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

/// Case-insensitive substring test (does `haystack` contain `needle`?). An
/// empty needle matches everything.
inline bool ci_contains(std::string_view haystack, std::string_view needle) {
    return to_lower_ascii(haystack).find(to_lower_ascii(needle)) != std::string::npos;
}

/// Case-insensitive full-string equality.
inline bool ci_equals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && to_lower_ascii(a) == to_lower_ascii(b);
}

}  // namespace pfdb::query
