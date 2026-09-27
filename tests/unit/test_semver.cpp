#include <catch2/catch_test_macros.hpp>

#include "app/version.hpp"

using namespace pfdb::app;

TEST_CASE("semver parses core versions", "[semver]") {
    const auto v = parse_semver("1.2.3");
    REQUIRE(v.has_value());
    REQUIRE(v->major == 1);
    REQUIRE(v->minor == 2);
    REQUIRE(v->patch == 3);
    REQUIRE(v->pre.empty());
    REQUIRE(v->str() == "1.2.3");
}

TEST_CASE("semver tolerates a leading v and build metadata", "[semver]") {
    const auto v = parse_semver("v2.0.0+build.5");
    REQUIRE(v.has_value());
    REQUIRE(v->major == 2);
    REQUIRE(v->pre.empty());  // build metadata is ignored
}

TEST_CASE("semver parses a pre-release tag", "[semver]") {
    const auto v = parse_semver("1.0.0-rc.1");
    REQUIRE(v.has_value());
    REQUIRE(v->pre == "rc.1");
    REQUIRE(v->str() == "1.0.0-rc.1");
}

TEST_CASE("semver rejects malformed input", "[semver]") {
    REQUIRE_FALSE(parse_semver("1.2").has_value());
    REQUIRE_FALSE(parse_semver("").has_value());
    REQUIRE_FALSE(parse_semver("x.y.z").has_value());
    REQUIRE_FALSE(parse_semver("1.2.3.4").has_value());
    REQUIRE_FALSE(parse_semver("1.2.").has_value());
}

TEST_CASE("semver orders by precedence", "[semver]") {
    REQUIRE(*parse_semver("1.0.0") < *parse_semver("1.0.1"));
    REQUIRE(*parse_semver("1.0.0") < *parse_semver("1.1.0"));
    REQUIRE(*parse_semver("1.9.9") < *parse_semver("2.0.0"));
    REQUIRE(*parse_semver("1.0.0") == *parse_semver("1.0.0"));
    REQUIRE(*parse_semver("2.0.0") > *parse_semver("1.9.9"));
}

TEST_CASE("semver pre-release precedence", "[semver]") {
    // A pre-release is lower than its release.
    REQUIRE(*parse_semver("1.0.0-rc.1") < *parse_semver("1.0.0"));
    // Alphabetic ordering of identifiers.
    REQUIRE(*parse_semver("1.0.0-alpha") < *parse_semver("1.0.0-beta"));
    // Numeric identifiers compare numerically.
    REQUIRE(*parse_semver("1.0.0-rc.2") < *parse_semver("1.0.0-rc.10"));
    // A longer identifier list wins on a tie prefix.
    REQUIRE(*parse_semver("1.0.0-rc") < *parse_semver("1.0.0-rc.1"));
}
