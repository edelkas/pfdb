#include <catch2/catch_test_macros.hpp>

#include "net/http_client.hpp"
#include "sources/imdb/imdb_source.hpp"

using namespace pfdb;
using namespace pfdb::sources::imdb;

// This test really hits IMDb. The leading '.' in the tag hides it from the
// default run (CTest/CI stay hermetic); run it on demand to detect upstream
// breakage:  pfdb_tests "[.imdb-live]"
TEST_CASE("live IMDb search + fetch", "[.imdb-live]") {
    net::CprHttpClient http;
    ImdbSource src(http);

    const auto results = src.search("blade runner");
    REQUIRE_FALSE(results.empty());
    REQUIRE(results[0].external_id == "tt0083658");

    const Film f = src.fetch("tt0083658").film;
    REQUIRE(f.title == "Blade Runner");
    REQUIRE(f.year == 1982);
    REQUIRE_FALSE(f.credits.empty());
    REQUIRE(f.ratings.size() == 1);
}
