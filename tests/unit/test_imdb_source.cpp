#include <catch2/catch_test_macros.hpp>

#include "sources/imdb/imdb_source.hpp"
#include "support/fixtures.hpp"
#include "support/mock_http_client.hpp"

using namespace pfdb;
using namespace pfdb::sources;
using namespace pfdb::sources::imdb;

TEST_CASE("is_valid_title_id accepts only tt<digits>", "[imdb][source]") {
    REQUIRE(is_valid_title_id("tt0083658"));
    REQUIRE_FALSE(is_valid_title_id("nm0000631"));
    REQUIRE_FALSE(is_valid_title_id("tt"));
    REQUIRE_FALSE(is_valid_title_id("tt12a3"));
    REQUIRE_FALSE(is_valid_title_id(""));
}

TEST_CASE("ImdbSource.search hits the suggestion endpoint", "[imdb][source]") {
    test::MockHttpClient http;
    http.on("v3.sg.media-imdb.com/suggestion", 200,
            test::read_fixture("imdb/suggestion_blade_runner.json"));

    ImdbSource src(http);
    const auto results = src.search("blade runner");

    REQUIRE_FALSE(results.empty());
    REQUIRE(results[0].external_id == "tt0083658");
    REQUIRE(http.calls.size() == 1);
    REQUIRE(http.calls[0].method == "GET");
    // Query is percent-encoded into the path.
    REQUIRE(http.calls[0].url.find("blade%20runner") != std::string::npos);
}

TEST_CASE("ImdbSource.search surfaces network failures", "[imdb][source]") {
    test::MockHttpClient http;  // no rule -> 404
    ImdbSource src(http);
    REQUIRE_THROWS_AS(src.search("whatever"), SourceError);
}

TEST_CASE("ImdbSource.fetch parses and stamps fetched_at", "[imdb][source]") {
    test::MockHttpClient http;
    http.on("caching.graphql.imdb.com", 200,
            test::read_fixture("imdb/title_tt0083658.graphql.json"));

    ImdbSource src(http);
    const Film f = src.fetch("tt0083658").film;

    REQUIRE(f.title == "Blade Runner");
    REQUIRE(f.source_refs.size() == 1);
    REQUIRE(f.source_refs[0].fetched_at.has_value());

    REQUIRE(http.calls.size() == 1);
    REQUIRE(http.calls[0].method == "POST");
    REQUIRE(http.calls[0].url.find("caching.graphql.imdb.com") != std::string::npos);
    // The GraphQL body carries the requested id.
    REQUIRE(http.calls[0].body.find("tt0083658") != std::string::npos);
}

TEST_CASE("ImdbSource.fetch rejects a bad id before any request", "[imdb][source]") {
    test::MockHttpClient http;
    ImdbSource src(http);
    REQUIRE_THROWS_AS(src.fetch("not-an-id"), SourceError);
    REQUIRE(http.calls.empty());
}

TEST_CASE("ImdbSource.fetch maps a missing title to NotFound", "[imdb][source]") {
    test::MockHttpClient http;
    http.on("caching.graphql.imdb.com", 200,
            test::read_fixture("imdb/title_notfound.graphql.json"));
    ImdbSource src(http);
    try {
        src.fetch("tt00000000");
        FAIL("expected SourceError");
    } catch (const SourceError& e) {
        REQUIRE(e.kind() == SourceError::Kind::NotFound);
    }
}
