#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "pfdb/film.hpp"
#include "query/parser.hpp"  // QueryError
#include "query/sort.hpp"

using namespace pfdb;
using namespace pfdb::query;

namespace {

Film make(Id id, std::string title, std::optional<int> year) {
    Film f;
    f.id = id;
    f.title = std::move(title);
    f.year = year;
    return f;
}

std::vector<Id> ids(const std::vector<const Film*>& v) {
    std::vector<Id> out;
    for (const Film* f : v) {
        out.push_back(f->id);
    }
    return out;
}

}  // namespace

TEST_CASE("sort by a single descending key", "[query][sort]") {
    std::vector<Film> films{make(1, "Alien", 1979), make(2, "Arrival", 2016),
                            make(3, "Blade Runner", 1982)};
    std::vector<const Film*> v{&films[0], &films[1], &films[2]};
    sort_films(v, parse_sort("year:desc"));
    REQUIRE(ids(v) == std::vector<Id>{2, 3, 1});
}

TEST_CASE("multi-key sort breaks ties with a second key", "[query][sort]") {
    std::vector<Film> films{make(1, "Dune", 2021), make(2, "Arrival", 2021),
                            make(3, "Alien", 1979)};
    std::vector<const Film*> v{&films[0], &films[1], &films[2]};
    // year desc, then title asc within the same year.
    sort_films(v, parse_sort("year:desc,title:asc"));
    REQUIRE(ids(v) == std::vector<Id>{2, 1, 3});  // 2021 Arrival, 2021 Dune, 1979 Alien
}

TEST_CASE("unset values sort last regardless of direction", "[query][sort]") {
    std::vector<Film> films{make(1, "A", std::nullopt), make(2, "B", 2000),
                            make(3, "C", 1990)};
    std::vector<const Film*> asc{&films[0], &films[1], &films[2]};
    sort_films(asc, parse_sort("year:asc"));
    REQUIRE(ids(asc) == std::vector<Id>{3, 2, 1});  // 1990, 2000, then unset last

    std::vector<const Film*> desc{&films[0], &films[1], &films[2]};
    sort_films(desc, parse_sort("year:desc"));
    REQUIRE(ids(desc) == std::vector<Id>{2, 3, 1});  // 2000, 1990, then unset still last
}

TEST_CASE("parse_sort rejects bad specs", "[query][sort]") {
    REQUIRE_THROWS_AS(parse_sort("nosuchfield"), QueryError);
    REQUIRE_THROWS_AS(parse_sort("genre"), QueryError);       // list field not sortable
    REQUIRE_THROWS_AS(parse_sort("year:sideways"), QueryError);
}
