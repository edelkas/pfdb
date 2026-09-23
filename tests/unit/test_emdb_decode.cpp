#include <catch2/catch_test_macros.hpp>

#include <string>

#include <nlohmann/json.hpp>

#include "io/emdb/emdb_decode.hpp"

using namespace pfdb::io::emdb;

namespace {

// Encode ASCII text as UTF-16LE bytes with a BOM (what emdb.dat looks like).
std::string utf16le(const std::string& ascii) {
    std::string out;
    out.push_back('\xff');
    out.push_back('\xfe');
    for (char c : ascii) {
        out.push_back(c);
        out.push_back('\0');
    }
    return out;
}

}  // namespace

TEST_CASE("bytes_to_json decodes UTF-16 and escapes control chars in strings",
          "[emdb][decode]") {
    // A value with a 0x1E record separator and a raw backslash (a file path).
    std::string content = "{\"a\":\"x";
    content += '\x1e';
    content += "y\\z\"}";

    const std::string json = bytes_to_json(utf16le(content));
    const auto j = nlohmann::json::parse(json);

    std::string expected = "x";
    expected += '\x1e';  // survives as a literal separator, ready to split
    expected += "y\\z";  // backslash preserved
    REQUIRE(j["a"].get<std::string>() == expected);
}

TEST_CASE("bytes_to_json preserves a raw newline inside a string value",
          "[emdb][decode]") {
    std::string content = "{\"plot\":\"line1\nline2\"}";
    const std::string json = bytes_to_json(utf16le(content));
    const auto j = nlohmann::json::parse(json);
    REQUIRE(j["plot"].get<std::string>() == "line1\nline2");
}

TEST_CASE("bytes_to_json accepts UTF-8 with a BOM", "[emdb][decode]") {
    std::string content = "\xEF\xBB\xBF{\"k\":\"v\"}";
    const auto j = nlohmann::json::parse(bytes_to_json(content));
    REQUIRE(j["k"].get<std::string>() == "v");
}
