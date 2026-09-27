#include "app/version.hpp"

#include <cctype>
#include <charconv>
#include <vector>

#ifndef PFDB_VERSION
#define PFDB_VERSION "0.0.0-dev"
#endif

namespace pfdb::app {
namespace {

/// Split a dot-separated pre-release tag into its identifiers.
std::vector<std::string> split_ids(const std::string& pre) {
    std::vector<std::string> ids;
    std::string cur;
    for (char c : pre) {
        if (c == '.') {
            ids.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    ids.push_back(cur);
    return ids;
}

bool is_all_digits(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

/// SemVer pre-release precedence: numeric identifiers compare numerically and
/// rank below alphanumeric ones; a longer list wins when the shared prefix ties.
int compare_pre(const std::string& a, const std::string& b) {
    if (a == b) {
        return 0;
    }
    // An empty pre-release (a release) has *higher* precedence than any set one.
    if (a.empty()) {
        return 1;
    }
    if (b.empty()) {
        return -1;
    }
    const std::vector<std::string> ia = split_ids(a);
    const std::vector<std::string> ib = split_ids(b);
    const std::size_t n = std::min(ia.size(), ib.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (ia[i] == ib[i]) {
            continue;
        }
        const bool na = is_all_digits(ia[i]);
        const bool nb = is_all_digits(ib[i]);
        if (na && nb) {
            const unsigned long va = std::stoul(ia[i]);
            const unsigned long vb = std::stoul(ib[i]);
            return va < vb ? -1 : 1;
        }
        if (na != nb) {
            return na ? -1 : 1;  // numeric < alphanumeric
        }
        return ia[i] < ib[i] ? -1 : 1;
    }
    if (ia.size() == ib.size()) {
        return 0;
    }
    return ia.size() < ib.size() ? -1 : 1;
}

}  // namespace

bool operator==(const SemVer& a, const SemVer& b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch &&
           a.pre == b.pre;
}

bool operator<(const SemVer& a, const SemVer& b) {
    if (a.major != b.major) {
        return a.major < b.major;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch;
    }
    return compare_pre(a.pre, b.pre) < 0;
}

std::string SemVer::str() const {
    std::string s = std::to_string(major) + '.' + std::to_string(minor) + '.' +
                    std::to_string(patch);
    if (!pre.empty()) {
        s += '-';
        s += pre;
    }
    return s;
}

std::optional<SemVer> parse_semver(std::string_view text) {
    std::size_t i = 0;
    if (i < text.size() && (text[i] == 'v' || text[i] == 'V')) {
        ++i;
    }

    // Strip build metadata ("+...") — parsed away, ignored for ordering.
    if (const auto plus = text.find('+', i); plus != std::string_view::npos) {
        text = text.substr(0, plus);
    }

    std::string pre;
    if (const auto dash = text.find('-', i); dash != std::string_view::npos) {
        pre = std::string(text.substr(dash + 1));
        text = text.substr(0, dash);
    }

    auto read_number = [&](int& out) -> bool {
        const std::size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
            ++i;
        }
        if (i == start) {
            return false;
        }
        int value = 0;
        const auto* first = text.data() + start;
        const auto* last = text.data() + i;
        const auto res = std::from_chars(first, last, value);
        if (res.ec != std::errc{} || res.ptr != last) {
            return false;
        }
        out = value;
        return true;
    };

    SemVer v;
    if (!read_number(v.major)) {
        return std::nullopt;
    }
    if (i >= text.size() || text[i] != '.') {
        return std::nullopt;
    }
    ++i;
    if (!read_number(v.minor)) {
        return std::nullopt;
    }
    if (i >= text.size() || text[i] != '.') {
        return std::nullopt;
    }
    ++i;
    if (!read_number(v.patch)) {
        return std::nullopt;
    }
    if (i != text.size()) {
        return std::nullopt;  // trailing junk
    }
    v.pre = std::move(pre);
    return v;
}

SemVer current_version() {
    if (auto v = parse_semver(PFDB_VERSION)) {
        return *v;
    }
    return SemVer{};
}

}  // namespace pfdb::app
