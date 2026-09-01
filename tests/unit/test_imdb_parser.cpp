#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sources/imdb/imdb_parser.hpp"
#include "support/fixtures.hpp"

using namespace pfdb;
using namespace pfdb::sources;
using namespace pfdb::sources::imdb;

TEST_CASE("parse_suggestions extracts title hits", "[imdb][parser]") {
    const std::string json = test::read_fixture("imdb/suggestion_blade_runner.json");
    const std::vector<SearchResult> results = parse_suggestions(json);

    REQUIRE(results.size() >= 3);
    REQUIRE(results[0].source == "imdb");
    REQUIRE(results[0].external_id == "tt0083658");
    REQUIRE(results[0].title == "Blade Runner");
    REQUIRE(results[0].year == 1982);
    REQUIRE(results[0].type == "feature");
    REQUIRE_FALSE(results[0].image_url.empty());
    // Only title entries are kept.
    for (const auto& r : results) {
        REQUIRE(r.external_id.rfind("tt", 0) == 0);
    }
}

TEST_CASE("parse_suggestions on empty/garbage input", "[imdb][parser]") {
    REQUIRE(parse_suggestions(R"({"d":[]})").empty());
    REQUIRE_THROWS_AS(parse_suggestions("not json"), SourceError);
}

TEST_CASE("parse_title maps GraphQL to a Film", "[imdb][parser]") {
    const std::string json = test::read_fixture("imdb/title_tt0083658.graphql.json");
    const Film f = parse_title(json);

    REQUIRE(f.title == "Blade Runner");
    REQUIRE(f.year == 1982);
    REQUIRE(f.runtime_minutes == 117);
    REQUIRE_FALSE(f.synopsis.empty());
    REQUIRE_FALSE(f.genres.empty());

    REQUIRE(f.ratings.size() == 1);
    REQUIRE(f.ratings[0].source == "imdb");
    REQUIRE(f.ratings[0].value == Catch::Approx(8.1));
    REQUIRE(f.ratings[0].scale == Catch::Approx(10.0));
    REQUIRE(f.ratings[0].votes.has_value());

    bool has_director = false;
    for (const auto& c : f.credits) {
        if (c.role == CreditRole::Director && c.person.name == "Ridley Scott") {
            has_director = true;
        }
    }
    REQUIRE(has_director);

    // The parser records the source id but not fetched_at (no clock in a pure parse).
    REQUIRE(f.source_refs.size() == 1);
    REQUIRE(f.source_refs[0].source == "imdb");
    REQUIRE(f.source_refs[0].external_id == "tt0083658");
    REQUIRE_FALSE(f.source_refs[0].fetched_at.has_value());
}

TEST_CASE("parse_title throws NotFound when the title is empty", "[imdb][parser]") {
    const std::string json = test::read_fixture("imdb/title_notfound.graphql.json");
    REQUIRE_THROWS_AS(parse_title(json), SourceError);
    try {
        parse_title(json);
    } catch (const SourceError& e) {
        REQUIRE(e.kind() == SourceError::Kind::NotFound);
    }
}
