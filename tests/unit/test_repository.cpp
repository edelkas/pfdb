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
    f.budget = 28000000;
    f.gross = 41600000;
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
    f.user.watch_count = 3;
    f.user.owned = true;
    f.user.wishlist = false;
    VideoFileInfo v;
    v.path = "/movies/blade_runner.mkv";
    v.size_bytes = std::int64_t{8'000'000'000};
    v.duration_seconds = 7020;
    v.width = 1920;
    v.height = 1080;
    v.codec = "mkv/h265";
    v.framerate = 23.976;
    v.video_bitrate = 12000000;
    v.audio_tracks = {
        {"Main", "English", "AC-3", std::int64_t{500'000'000}, 640000, 6, 48000},
        {"Commentary", "English", "AAC", std::nullopt, 128000, 2, 48000},
    };
    v.subtitle_tracks = {
        {"Forced", "English", "PGS", std::int64_t{2'000'000}},
        {"Full", "Spanish", "SRT", std::nullopt},
    };
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
    // People are assigned ids on insert, mirrored back on load (like the film id).
    REQUIRE(expected.credits.size() == loaded->credits.size());
    for (std::size_t i = 0; i < expected.credits.size(); ++i) {
        REQUIRE(loaded->credits[i].person.id > 0);
        expected.credits[i].person.id = loaded->credits[i].person.id;
    }
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

TEST_CASE("FilmAffinity fields, topics and groups round-trip", "[db]") {
    db::Repository repo(":memory:");
    Film f;
    f.title = "Blade Runner";
    f.spanish_title = "Blade Runner";
    f.spanish_synopsis = "Noviembre de 2019...";
    f.review_count = 642;
    f.topics = {"Neo-noir", "Thriller futurista"};
    f.groups = {"Adaptaciones de Philip K. Dick"};

    const Id id = repo.insert(f);
    const auto loaded = repo.find(id);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->spanish_title == "Blade Runner");
    REQUIRE(loaded->spanish_synopsis == "Noviembre de 2019...");
    REQUIRE(loaded->review_count == 642);
    REQUIRE(loaded->topics == std::vector<std::string>{"Neo-noir", "Thriller futurista"});
    REQUIRE(loaded->groups == std::vector<std::string>{"Adaptaciones de Philip K. Dick"});
}

TEST_CASE("find_id_by_source_ref locates a film by external id", "[db]") {
    db::Repository repo(":memory:");
    Film f;
    f.title = "Blade Runner";
    f.source_refs.push_back({"filmaffinity", "358476", std::nullopt});
    const Id id = repo.insert(f);

    REQUIRE(repo.find_id_by_source_ref("filmaffinity", "358476") == id);
    REQUIRE_FALSE(repo.find_id_by_source_ref("filmaffinity", "999").has_value());
    REQUIRE_FALSE(repo.find_id_by_source_ref("imdb", "358476").has_value());
}

TEST_CASE("relation and similarity edges store, read and cascade", "[db]") {
    db::Repository repo(":memory:");
    Film a;
    a.title = "Blade Runner";
    const Id ia = repo.insert(a);
    Film b;
    b.title = "Blade Runner 2049";
    const Id ib = repo.insert(b);

    repo.replace_relations(ia, {{ib, "tiene secuela"}});
    const auto rels = repo.relations_of(ia);
    REQUIRE(rels.size() == 1);
    REQUIRE(rels[0].other_id == ib);
    REQUIRE(rels[0].kind == "tiene secuela");

    repo.replace_similarities(ia, {{ib, 80}});
    const auto sims = repo.similarities_of(ia);
    REQUIRE(sims.size() == 1);
    REQUIRE(sims[0].other_id == ib);
    REQUIRE(sims[0].percent == 80);
    // Similarity is undirected: readable from the other endpoint too.
    const auto sims_b = repo.similarities_of(ib);
    REQUIRE(sims_b.size() == 1);
    REQUIRE(sims_b[0].other_id == ia);

    // Deleting a film cascades its edges away.
    REQUIRE(repo.remove(ia));
    REQUIRE(repo.relations_of(ia).empty());
    REQUIRE(repo.similarities_of(ib).empty());
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
    REQUIRE(all[0].video->audio_tracks.size() == 2);
    REQUIRE(all[0].video->subtitle_tracks.size() == 2);
    REQUIRE(all[0].budget == 28000000);
    REQUIRE(all[1].title == "Alien");
    REQUIRE(all[1].credits.empty());
}

TEST_CASE("covers are stored, fetched, and cascade-deleted", "[db]") {
    db::Repository repo(":memory:");
    const Id id = repo.insert(sample_film());

    REQUIRE_FALSE(repo.has_cover(id));
    REQUIRE_FALSE(repo.get_cover(id).has_value());

    const std::string bytes("\x89PNG\r\n\x1a\n\x00\x01\x02", 11);  // binary, with a NUL
    repo.set_cover(id, "image/png", bytes);

    REQUIRE(repo.has_cover(id));
    const auto cover = repo.get_cover(id);
    REQUIRE(cover.has_value());
    REQUIRE(cover->mime == "image/png");
    REQUIRE(cover->bytes == bytes);  // exact bytes, including the embedded NUL
    REQUIRE(repo.ids_with_cover() == std::vector<Id>{id});

    // Covers survive a film update (they are not part of the Film value).
    REQUIRE(repo.update(*repo.find(id)));
    REQUIRE(repo.has_cover(id));

    // ... but are cascade-deleted with the film.
    REQUIRE(repo.remove(id));
    REQUIRE_FALSE(repo.has_cover(id));
}
