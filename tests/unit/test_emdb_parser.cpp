#include <catch2/catch_test_macros.hpp>

#include <string>

#include "io/emdb/emdb_decode.hpp"
#include "io/emdb/emdb_parser.hpp"
#include "support/fixtures.hpp"

using namespace pfdb::io::emdb;

namespace {

EmdbData load_sample() {
    return parse(bytes_to_json(pfdb::test::read_fixture("emdb/sample.dat")));
}

}  // namespace

TEST_CASE("emdb parser reads the lookup tables", "[emdb][parser]") {
    const EmdbData d = load_sample();
    REQUIRE(d.version == 80);
    REQUIRE(d.actors.size() == 2);
    REQUIRE(d.actors[0].name == "Harrison Ford");
    REQUIRE(d.actors[0].id == "0000148");
    REQUIRE(d.directors.size() == 1);
    REQUIRE(d.directors[0].name == "Ridley Scott");
    REQUIRE(d.writers[0].name == "Hampton Fancher");
    REQUIRE(d.composers[0].name == "Vangelis");
    REQUIRE(d.tags.size() == 2);
    REQUIRE(d.tags[0] == "Cyberpunk");
    REQUIRE(d.collections.at("1000") == "Blade Runner Collection");
    REQUIRE(d.custom_genres.at("00") == "Giallo");
    REQUIRE(d.movies.size() == 2);
}

TEST_CASE("emdb parser splits movie groups and un-tags values", "[emdb][parser]") {
    const EmdbData d = load_sample();
    const EmdbMovie& m = d.movies[0];

    REQUIRE(m.field("title", 0) == "Blade Runner");
    REQUIRE(m.field("title", 3) == "1000");            // collection id
    REQUIRE(m.field("year", 2) == "117");              // runtime
    REQUIRE(m.field("cast", 0) == "0,1");              // actor indexes
    REQUIRE(m.field("cast", 1) == "Rick Deckard|Roy Batty");
    REQUIRE(m.field("genres", 1) == "0083658|0");      // imdb id | top250
    // <DQ> tags are decoded back to literal quotes.
    REQUIRE(m.field("comments", 0) == "Great \"tears in rain\" monologue.");
    // Absent group/index yields "".
    REQUIRE(m.field("nope", 0).empty());
    REQUIRE(m.field("title", 99).empty());
}
