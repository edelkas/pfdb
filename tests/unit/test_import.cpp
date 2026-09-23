#include <catch2/catch_test_macros.hpp>

#include <string>

#include "app/import.hpp"
#include "db/repository.hpp"
#include "io/field_set.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

namespace {

std::string sample_path() {
    return std::string(PFDB_FIXTURES_DIR) + "/emdb/sample.dat";
}

const Film* find_by_imdb(const std::vector<Film>& films, const std::string& ext) {
    for (const auto& f : films) {
        for (const auto& r : f.source_refs) {
            if (r.source == "imdb" && r.external_id == ext) {
                return &f;
            }
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("import dry-run reports counts without writing", "[import]") {
    db::Repository repo(":memory:");
    const app::ImportStats stats =
        app::import_emdb(repo, sample_path(), io::FieldSet::userdata(), /*dry_run=*/true);
    REQUIRE(stats.total == 2);
    REQUIRE(stats.added == 2);
    REQUIRE(stats.updated == 0);
    REQUIRE(repo.count() == 0);
}

TEST_CASE("import inserts stubs then upserts by imdb id", "[import]") {
    db::Repository repo(":memory:");

    auto first = app::import_emdb(repo, sample_path(), io::FieldSet::userdata(), false);
    REQUIRE(first.added == 2);
    REQUIRE(repo.count() == 2);

    const std::vector<Film> loaded = repo.load_all();
    const Film* bladerunner = find_by_imdb(loaded, "tt0083658");
    REQUIRE(bladerunner != nullptr);
    REQUIRE(bladerunner->user.watch_count == 3);
    REQUIRE(bladerunner->user.owned);
    REQUIRE(bladerunner->genres.empty());  // metadata not imported under userdata

    // Re-importing the same file updates the existing rows, not inserts.
    auto second = app::import_emdb(repo, sample_path(), io::FieldSet::userdata(), false);
    REQUIRE(second.added == 0);
    REQUIRE(second.updated == 2);
    REQUIRE(repo.count() == 2);
}

TEST_CASE("importing metadata preserves already-imported user data", "[import]") {
    db::Repository repo(":memory:");
    app::import_emdb(repo, sample_path(), io::FieldSet::userdata(), false);

    // Now bring in the metadata for the same films.
    auto stats = app::import_emdb(repo, sample_path(), io::FieldSet::metadata(), false);
    REQUIRE(stats.updated == 2);
    REQUIRE(repo.count() == 2);

    const std::vector<Film> loaded = repo.load_all();
    const Film* f = find_by_imdb(loaded, "tt0083658");
    REQUIRE(f != nullptr);
    REQUIRE(f->title == "Blade Runner");   // metadata now present
    REQUIRE_FALSE(f->genres.empty());
    REQUIRE(f->user.watch_count == 3);     // user data preserved
    REQUIRE(f->user.owned);
}
