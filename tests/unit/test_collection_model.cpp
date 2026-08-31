#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "model/collection_model.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

namespace {

Film make_film(Id id, std::string title, int year) {
    Film f;
    f.id = id;
    f.title = std::move(title);
    f.year = year;
    return f;
}

CollectionModel three_films() {
    std::vector<Film> films{
        make_film(1, "Blade Runner", 1982),
        make_film(2, "Alien", 1979),
        make_film(3, "Arrival", 2016),
    };
    return CollectionModel(std::move(films));
}

}  // namespace

TEST_CASE("model indexes films by id", "[model]") {
    const auto model = three_films();
    REQUIRE(model.size() == 3);
    REQUIRE(model.find(2) != nullptr);
    REQUIRE(model.find(2)->title == "Alien");
    REQUIRE(model.find(99) == nullptr);
}

TEST_CASE("filter selects matching films in load order", "[model]") {
    const auto model = three_films();
    const auto eighties =
        model.filter([](const Film& f) { return f.year && *f.year / 10 == 198; });
    REQUIRE(eighties.size() == 1);
    REQUIRE(eighties[0]->title == "Blade Runner");
}

TEST_CASE("upsert inserts new and replaces existing", "[model]") {
    auto model = three_films();

    model.upsert(make_film(4, "Dune", 2021));
    REQUIRE(model.size() == 4);
    REQUIRE(model.find(4)->title == "Dune");

    model.upsert(make_film(1, "Blade Runner 2049", 2017));
    REQUIRE(model.size() == 4);  // replaced, not added
    REQUIRE(model.find(1)->title == "Blade Runner 2049");
}

TEST_CASE("erase removes and keeps the index consistent", "[model]") {
    auto model = three_films();

    REQUIRE(model.erase(1));
    REQUIRE(model.size() == 2);
    REQUIRE(model.find(1) == nullptr);
    // Remaining films are still reachable after the swap-and-pop.
    REQUIRE(model.find(2)->title == "Alien");
    REQUIRE(model.find(3)->title == "Arrival");

    REQUIRE_FALSE(model.erase(1));  // already gone
}
