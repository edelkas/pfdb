#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sources/filmaffinity/fa_parser.hpp"
#include "support/fixtures.hpp"

using namespace pfdb;
using namespace pfdb::sources;
using namespace pfdb::sources::filmaffinity;

namespace {
bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}
}  // namespace

TEST_CASE("fa parse_film maps FilmAffinity fields", "[fa][parser]") {
    const std::string html = test::read_fixture("fa/film358476.html");
    const FilmParse p = parse_film(html);
    const Film& f = p.film;

    REQUIRE(f.spanish_title == "Blade Runner");
    REQUIRE_FALSE(f.spanish_synopsis.empty());
    REQUIRE(f.review_count == 642);

    REQUIRE(f.ratings.size() == 1);
    REQUIRE(f.ratings[0].source == "filmaffinity");
    REQUIRE(f.ratings[0].value == Catch::Approx(8.1));
    REQUIRE(f.ratings[0].votes.has_value());
    REQUIRE(*f.ratings[0].votes > 100000);

    REQUIRE(contains(f.topics, "Neo-noir"));
    REQUIRE(contains(f.groups, "Adaptaciones de Philip K. Dick"));

    // Similar movies, with Blade Runner 2049 the closest at 80%.
    REQUIRE(p.similars.size() >= 10);
    bool found_2049 = false;
    for (const auto& s : p.similars) {
        if (s.external_id == "236626") {
            found_2049 = true;
            REQUIRE(s.percent == 80);
        }
    }
    REQUIRE(found_2049);
}

TEST_CASE("fa parse_relations groups by relationship type", "[fa][parser]") {
    const std::string html = test::read_fixture("fa/relations_358476.html");
    const auto rels = parse_relations(html);

    REQUIRE(rels.size() == 10);
    bool found_sequel = false;
    for (const auto& r : rels) {
        if (r.external_id == "236626") {
            found_sequel = true;
            REQUIRE(r.kind.find("secuela") != std::string::npos);
        }
    }
    REQUIRE(found_sequel);
}

TEST_CASE("fa parse_search extracts titles and ids", "[fa][parser]") {
    const std::string html = test::read_fixture("fa/search_blade_runner.html");
    const auto results = parse_search(html);

    REQUIRE_FALSE(results.empty());
    bool found = false;
    for (const auto& r : results) {
        if (r.external_id == "358476") {
            found = true;
            REQUIRE(r.title == "Blade Runner");
            REQUIRE(r.source == "filmaffinity");
        }
    }
    REQUIRE(found);
}

TEST_CASE("fa parse_film throws NotFound on a title-less page", "[fa][parser]") {
    REQUIRE_THROWS_AS(parse_film("<html><body><p>nope</p></body></html>"), SourceError);
}
