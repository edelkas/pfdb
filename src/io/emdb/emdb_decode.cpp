#include "io/emdb/emdb_decode.hpp"

#include <array>
#include <cstdint>

namespace pfdb::io::emdb {
namespace {

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string utf16le_to_utf8(std::string_view bytes, std::size_t start) {
    std::string out;
    out.reserve(bytes.size());
    for (std::size_t i = start; i + 1 < bytes.size(); i += 2) {
        const auto lo = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i]));
        const auto hi = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 1]));
        std::uint32_t unit = lo | (hi << 8);
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < bytes.size()) {
            const auto lo2 =
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 2]));
            const auto hi2 =
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 3]));
            const std::uint32_t low = lo2 | (hi2 << 8);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                const std::uint32_t cp =
                    0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                append_utf8(out, cp);
                i += 2;
                continue;
            }
        }
        append_utf8(out, unit);
    }
    return out;
}

std::string sanitize(std::string_view utf8) {
    static constexpr std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string out;
    out.reserve(utf8.size() + 16);
    bool in_string = false;
    for (char ch : utf8) {
        const auto c = static_cast<unsigned char>(ch);
        if (!in_string) {
            out.push_back(ch);
            if (ch == '"') {
                in_string = true;
            }
            continue;
        }
        // Inside a string value.
        if (ch == '"') {
            in_string = false;
            out.push_back(ch);
        } else if (ch == '\\') {
            out += "\\\\";
        } else if (c < 0x20) {
            out += "\\u00";
            out.push_back(kHex[(c >> 4) & 0xF]);
            out.push_back(kHex[c & 0xF]);
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace

std::string bytes_to_json(std::string_view raw) {
    // UTF-8 BOM: already UTF-8, just sanitize the remainder.
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB &&
        static_cast<unsigned char>(raw[2]) == 0xBF) {
        return sanitize(raw.substr(3));
    }
    // UTF-16LE, with or without a BOM.
    std::size_t start = 0;
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
        static_cast<unsigned char>(raw[1]) == 0xFE) {
        start = 2;
    }
    return sanitize(utf16le_to_utf8(raw, start));
}

}  // namespace pfdb::io::emdb
