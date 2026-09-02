#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "model/collection_model.hpp"
#include "pfdb/film.hpp"
#include "query/engine.hpp"
#include "query/parser.hpp"

using namespace pfdb;
using namespace pfdb::query;

namespace {

Film film(Id id, std::string title, int year, std::string genre, Id actor_id,
          std::string actor) {
    Film f;
    f.id = id;
    f.title = std::move(title);
    f.year = year;
    f.genres = {std::move(genre)};
    Credit c;
    c.person = Person{actor_id, std::move(actor)};
    c.role = CreditRole::Actor;
    f.credits.push_back(c);
    return f;
}

CollectionModel sample() {
    std::vector<Film> films{
        film(1, "Blade Runner", 1982, "Sci-Fi", 42, "Harrison Ford"),
        film(2, "Blade Runner 2049", 2017, "Sci-Fi", 50, "Ryan Gosling"),
        film(3, "Alien", 1979, "Horror", 60, "Sigourney Weaver"),
    };
    CollectionModel::EdgeIndex related{{1, {2}}, {2, {1}}};
    CollectionModel::EdgeIndex similar{{1, {2}}, {2, {1}}};
    return CollectionModel(std::move(films), std::move(related), std::move(similar));
}

std::vector<Id> ids(const std::vector<const Film*>& v) {
    std::vector<Id> out;
    for (const Film* f : v) {
        out.push_back(f->id);
    }
    return out;
}

}  // namespace

TEST_CASE("run_query with no options returns everything in load order", "[query][engine]") {
    const auto model = sample();
    REQUIRE(ids(run_query(model, {})) == std::vector<Id>{1, 2, 3});
}

TEST_CASE("run_query applies a single filter", "[query][engine]") {
    const auto model = sample();
    QueryRequest req;
    req.filter_specs = {"title ~ blade"};
    REQUIRE(ids(run_query(model, req)) == std::vector<Id>{1, 2});
}

TEST_CASE("run_query combines filters with a boolean expression", "[query][engine]") {
    const auto model = sample();
    QueryRequest req;
    req.filter_specs = {"genre has Sci-Fi", "year >= 2000"};
    req.where = "F1 AND F2";
    REQUIRE(ids(run_query(model, req)) == std::vector<Id>{2});

    req.where = "F1 AND NOT F2";
    REQUIRE(ids(run_query(model, req)) == std::vector<Id>{1});
}

TEST_CASE("run_query resolves cast names to ids", "[query][engine]") {
    const auto model = sample();
    QueryRequest req;
    req.filter_specs = {"cast has Harrison"};
    REQUIRE(ids(run_query(model, req)) == std::vector<Id>{1});
}

TEST_CASE("run_query reads relation/similarity edges", "[query][engine]") {
    const auto model = sample();
    QueryRequest related;
    related.filter_specs = {"related_to has 1"};
    REQUIRE(ids(run_query(model, related)) == std::vector<Id>{2});

    QueryRequest similar;
    similar.filter_specs = {"similar_to has 2"};
    REQUIRE(ids(run_query(model, similar)) == std::vector<Id>{1});
}

TEST_CASE("run_query sorts the result set", "[query][engine]") {
    const auto model = sample();
    QueryRequest req;
    req.sort = "year:desc";
    REQUIRE(ids(run_query(model, req)) == std::vector<Id>{2, 1, 3});
}

TEST_CASE("run_query surfaces parse errors", "[query][engine]") {
    const auto model = sample();
    QueryRequest req;
    req.filter_specs = {"nosuchfield ~ x"};
    REQUIRE_THROWS_AS(run_query(model, req), QueryError);
}
