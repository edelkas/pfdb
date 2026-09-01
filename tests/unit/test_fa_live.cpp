#include <catch2/catch_test_macros.hpp>

#include "net/http_client.hpp"
#include "sources/filmaffinity/fa_source.hpp"

using namespace pfdb;
using namespace pfdb::sources;
using namespace pfdb::sources::filmaffinity;

// Really hits FilmAffinity. Hidden from the default run (leading '.'); run on
// demand to detect upstream breakage:  pfdb_tests "[.fa-live]"
TEST_CASE("live FilmAffinity search + fetch", "[.fa-live]") {
    net::CprHttpClient http;
    FaSource src(http);

    const auto results = src.search("blade runner");
    REQUIRE_FALSE(results.empty());

    const SourceFetch out = src.fetch("358476");
    REQUIRE(out.film.spanish_title == "Blade Runner");
    REQUIRE(out.film.review_count.has_value());
    REQUIRE_FALSE(out.relations.empty());
    REQUIRE_FALSE(out.similars.empty());
}
