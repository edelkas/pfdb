#include <catch2/catch_test_macros.hpp>

#include "sources/filmaffinity/fa_source.hpp"
#include "support/fixtures.hpp"
#include "support/mock_http_client.hpp"

using namespace pfdb;
using namespace pfdb::sources;
using namespace pfdb::sources::filmaffinity;

TEST_CASE("is_valid_fa_id accepts only digits", "[fa][source]") {
    REQUIRE(is_valid_fa_id("358476"));
    REQUIRE_FALSE(is_valid_fa_id("tt358476"));
    REQUIRE_FALSE(is_valid_fa_id(""));
}

TEST_CASE("FaSource.fetch combines film page and relations page", "[fa][source]") {
    test::MockHttpClient http;
    http.on("/film358476.html", 200, test::read_fixture("fa/film358476.html"));
    http.on("movie-relations.php?movie-id=358476", 200,
            test::read_fixture("fa/relations_358476.html"));

    FaSource src(http);
    const SourceFetch out = src.fetch("358476");

    REQUIRE(out.film.spanish_title == "Blade Runner");
    REQUIRE(out.film.source_refs.size() == 1);
    REQUIRE(out.film.source_refs[0].source == "filmaffinity");
    REQUIRE(out.film.source_refs[0].external_id == "358476");
    REQUIRE(out.film.source_refs[0].fetched_at.has_value());

    REQUIRE(out.relations.size() == 10);
    REQUIRE(out.similars.size() >= 10);

    // Two GETs: film page then relations page.
    REQUIRE(http.calls.size() == 2);
    REQUIRE(http.calls[0].method == "GET");
}

TEST_CASE("FaSource.search parses the results page", "[fa][source]") {
    test::MockHttpClient http;
    http.on("search.php", 200, test::read_fixture("fa/search_blade_runner.html"));
    FaSource src(http);
    REQUIRE_FALSE(src.search("blade runner").empty());
}

TEST_CASE("FaSource.fetch rejects a non-numeric id before any request", "[fa][source]") {
    test::MockHttpClient http;
    FaSource src(http);
    REQUIRE_THROWS_AS(src.fetch("tt0083658"), SourceError);
    REQUIRE(http.calls.empty());
}
