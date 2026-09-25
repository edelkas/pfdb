#include <catch2/catch_test_macros.hpp>

#include "sources/boxofficemojo/bom_source.hpp"
#include "support/fixtures.hpp"
#include "support/mock_http_client.hpp"

using namespace pfdb;
using namespace pfdb::sources;

TEST_CASE("bom source fetches financials into a film", "[bom]") {
    test::MockHttpClient http;
    http.on("boxofficemojo.com/title/tt1856101",
            200, test::read_fixture("bom/title_tt1856101.html"));

    boxofficemojo::BomSource src(http);
    const SourceFetch out = src.fetch("tt1856101");

    REQUIRE(out.film.budget == 150000000);
    REQUIRE(out.film.gross == 277882781);
    REQUIRE(out.film.source_refs.size() == 1);
    REQUIRE(out.film.source_refs[0].source == "boxofficemojo");
    REQUIRE(out.film.source_refs[0].external_id == "tt1856101");
}

TEST_CASE("bom source rejects a non-IMDb id", "[bom]") {
    test::MockHttpClient http;
    boxofficemojo::BomSource src(http);
    REQUIRE_THROWS_AS(src.fetch("358476"), SourceError);
}
