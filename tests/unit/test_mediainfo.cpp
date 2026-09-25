#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "media/mediainfo.hpp"

using namespace pfdb::media;
using Catch::Approx;

TEST_CASE("parse_int_loose handles grouping spaces and units", "[mediainfo]") {
    REQUIRE(parse_int_loose("1 920") == 1920);
    REQUIRE(parse_int_loose("5 000 000") == 5000000);
    REQUIRE(parse_int_loose("48000") == 48000);
    REQUIRE(parse_int_loose("192 000 bps") == 192000);
    REQUIRE(parse_int_loose("23.976") == 23);  // stops at the decimal point
    REQUIRE_FALSE(parse_int_loose("n/a").has_value());
    REQUIRE_FALSE(parse_int_loose("").has_value());
}

TEST_CASE("parse_double_loose handles decimals and spaces", "[mediainfo]") {
    REQUIRE(*parse_double_loose("23.976") == Approx(23.976));
    REQUIRE(*parse_double_loose("6120000") == Approx(6120000.0));
    REQUIRE(*parse_double_loose("29.97 fps") == Approx(29.97));
    REQUIRE_FALSE(parse_double_loose("").has_value());
}
