#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "pfdb/film.hpp"
#include "query/eval_context.hpp"
#include "query/filter.hpp"
#include "query/parser.hpp"

using namespace pfdb;
using namespace pfdb::query;

namespace {

Film blade_runner() {
    Film f;
    f.id = 1;
    f.title = "Blade Runner 2049";
    f.year = 2017;
    f.runtime_minutes = 164;
    f.genres = {"Sci-Fi", "Drama"};
    f.topics = {"Neo-noir"};
    f.user.date_watched = "2024-05-01";
    f.user.personal_rating = 9.0;
    f.ratings.push_back({"imdb", 8.0, 10.0, std::int64_t{600000}});
    Credit c;
    c.person = Person{42, "Harrison Ford"};
    c.role = CreditRole::Actor;
    f.credits.push_back(c);
    return f;
}

bool m(const char* spec, const Film& f) {
    const EvalContext ctx;  // no edges needed here
    return matches(parse_filter(spec), f, ctx);
}

}  // namespace

TEST_CASE("textual filters: substring and regex", "[query][filter]") {
    const Film f = blade_runner();
    REQUIRE(m("title ~ blade", f));       // case-insensitive substring
    REQUIRE(m("title ~ BLADE", f));
    REQUIRE_FALSE(m("title ~ alien", f));
    REQUIRE(m(R"(title =~ blade.+\d+)", f));  // regex narrows to the numbered one
    REQUIRE_FALSE(m(R"(title =~ ^2049)", f));
}

TEST_CASE("numeric filters: range, equality, open ends", "[query][filter]") {
    const Film f = blade_runner();
    REQUIRE(m("year = 2017", f));
    REQUIRE_FALSE(m("year = 2016", f));
    REQUIRE(m("year = 2000..2020", f));
    REQUIRE_FALSE(m("year = 1990..1999", f));
    REQUIRE(m("runtime >= 120", f));
    REQUIRE_FALSE(m("runtime > 164", f));   // strict, excludes equality
    REQUIRE(m("runtime <= 164", f));
    REQUIRE(m("imdb_rating >= 8", f));
    REQUIRE(m("my_rating = 9", f));

    Film no_year = f;
    no_year.year.reset();
    REQUIRE_FALSE(m("year = 2017", no_year));  // unset field never matches a range
}

TEST_CASE("date filters compare chronologically", "[query][filter]") {
    const Film f = blade_runner();
    REQUIRE(m("date_watched = 2024-01-01..2024-12-31", f));
    REQUIRE(m("date_watched >= 2024-05-01", f));
    REQUIRE_FALSE(m("date_watched < 2024-05-01", f));
}

TEST_CASE("string inclusion matches list members exactly (case-insensitive)",
          "[query][filter]") {
    const Film f = blade_runner();
    REQUIRE(m("genre has Sci-Fi", f));
    REQUIRE(m("genre has sci-fi", f));
    REQUIRE(m("topic has Neo-noir", f));
    REQUIRE_FALSE(m("genre has Comedy", f));
    REQUIRE_FALSE(m("genre has Sci", f));  // exact element, not substring
}

TEST_CASE("id inclusion matches a person id in the credits", "[query][filter]") {
    const Film f = blade_runner();
    REQUIRE(m("cast_id has 42", f));
    REQUIRE_FALSE(m("cast_id has 7", f));
    REQUIRE_FALSE(m("director_id has 42", f));  // wrong role
}

TEST_CASE("related_to/similar_to read edges from the context", "[query][filter]") {
    const Film f = blade_runner();
    EvalContext ctx;
    std::unordered_map<Id, std::vector<Id>> related{{1, {2, 3}}};
    ctx.related = &related;
    REQUIRE(matches(parse_filter("related_to has 2"), f, ctx));
    REQUIRE_FALSE(matches(parse_filter("related_to has 9"), f, ctx));
    REQUIRE_FALSE(matches(parse_filter("similar_to has 2"), f, ctx));  // no similar edges
}
