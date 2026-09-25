#include <catch2/catch_test_macros.hpp>

#include "net/http_client.hpp"
#include "sources/boxofficemojo/bom_source.hpp"

using namespace pfdb;

// Hidden by default (hits the network). Run with: pfdb_tests "[.bom-live]".
TEST_CASE("boxofficemojo live fetch", "[.bom-live]") {
    net::CprHttpClient http;
    sources::boxofficemojo::BomSource src(http);
    const sources::SourceFetch out = src.fetch("tt1856101");
    REQUIRE(out.film.budget.has_value());
    REQUIRE(out.film.gross.has_value());
    REQUIRE(*out.film.gross > 0);
}
