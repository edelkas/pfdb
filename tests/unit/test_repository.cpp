#include <catch2/catch_test_macros.hpp>

#include "db/repository.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

namespace {

// A fully-populated film exercising every related table.
Film sample_film() {
    Film f;
    f.title = "Blade Runner";
    f.original_title = "Blade Runner";
    f.year = 1982;
    f.runtime_minutes = 117;
    f.synopsis = "A blade runner hunts replicants.";
    f.genres = {"Sci-Fi", "Thriller"};
    f.credits = {
        {Person{0, "Ridley Scott"}, CreditRole::Director, "", 0},
        {Person{0, "Harrison Ford"}, CreditRole::Actor, "Rick Deckard", 1},
    };
    f.ratings = {
        {"imdb", 8.1, 10.0, std::int64_t{800000}},
        {"filmaffinity", 8.0, 10.0, std::nullopt},
    };
    f.source_refs = {
        {"imdb", "tt0083658", std::nullopt},
    };
    f.user.date_watched = "2024-05-01";
    f.user.personal_rating = 9.0;
    f.user.notes = "Masterpiece.";
    f.user.favorite = true;
    VideoFileInfo v;
    v.path = "/movies/blade_runner.mkv";
    v.size_bytes = std::int64_t{8'000'000'000};
    v.duration_seconds = 7020;
    v.width = 1920;
    v.height = 1080;
    v.codec = "mkv/h265";
    f.video = v;
    return f;
}

}  // namespace

TEST_CASE("fresh database is at the latest schema version", "[db]") {
    db::Repository repo(":memory:");
    REQUIRE(repo.schema_version() == db::Repository::latest_schema_version());
    REQUIRE(repo.schema_version() >= 1);
    REQUIRE(repo.count() == 0);
}

TEST_CASE("insert then find round-trips all fields", "[db]") {
    db::Repository repo(":memory:");
    const Film original = sample_film();

    const Id id = repo.insert(original);
    REQUIRE(id > 0);
    REQUIRE(repo.count() == 1);

    const auto loaded = repo.find(id);
    REQUIRE(loaded.has_value());

    // Normalize the volatile fields the store assigns, then compare the rest.
    Film expected = original;
    expected.id = loaded->id;
    expected.created_at = loaded->created_at;
    expected.updated_at = loaded->updated_at;
    REQUIRE(*loaded == expected);

    REQUIRE(loaded->created_at.has_value());
    REQUIRE(loaded->updated_at.has_value());
}

TEST_CASE("find returns nullopt for a missing id", "[db]") {
    db::Repository repo(":memory:");
    REQUIRE_FALSE(repo.find(999).has_value());
}

TEST_CASE("update replaces scalar and related data", "[db]") {
    db::Repository repo(":memory:");
    const Id id = repo.insert(sample_film());

    Film edited = *repo.find(id);
    edited.title = "Blade Runner: The Final Cut";
    edited.genres = {"Sci-Fi"};  // dropped Thriller
    edited.ratings.clear();

    REQUIRE(repo.update(edited));

    const auto loaded = repo.find(id);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->title == "Blade Runner: The Final Cut");
    REQUIRE(loaded->genres == std::vector<std::string>{"Sci-Fi"});
    REQUIRE(loaded->ratings.empty());
}

TEST_CASE("update of a missing film reports failure", "[db]") {
    db::Repository repo(":memory:");
    Film ghost = sample_film();
    ghost.id = 12345;
    REQUIRE_FALSE(repo.update(ghost));
}

TEST_CASE("remove deletes the film and cascades", "[db]") {
    db::Repository repo(":memory:");
    const Id id = repo.insert(sample_film());

    REQUIRE(repo.remove(id));
    REQUIRE(repo.count() == 0);
    REQUIRE_FALSE(repo.find(id).has_value());
    REQUIRE_FALSE(repo.remove(id));  // second remove is a no-op
}

TEST_CASE("load_all returns every film fully populated", "[db]") {
    db::Repository repo(":memory:");
    repo.insert(sample_film());
    Film second;
    second.title = "Alien";
    second.year = 1979;
    repo.insert(second);

    const auto all = repo.load_all();
    REQUIRE(all.size() == 2);
    REQUIRE(all[0].title == "Blade Runner");
    REQUIRE(all[0].credits.size() == 2);
    REQUIRE(all[0].video.has_value());
    REQUIRE(all[1].title == "Alien");
    REQUIRE(all[1].credits.empty());
}
