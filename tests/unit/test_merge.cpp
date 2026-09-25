#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/enrichment.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

namespace {

Film imdb_film() {
    Film f;
    f.title = "Blade Runner";
    f.original_title = "Blade Runner";
    f.year = 1982;
    f.runtime_minutes = 117;
    f.synopsis = "A blade runner must pursue replicants.";
    f.genres = {"Sci-Fi", "Action"};
    f.ratings.push_back({"imdb", 8.1, 10.0, std::int64_t{800000}});
    f.source_refs.push_back({"imdb", "tt0083658", std::nullopt});
    return f;
}

Film fa_film() {
    Film f;
    f.title = "Blade Runner";
    f.synopsis = "Noviembre de 2019...";
    f.spanish_title = "Blade Runner";
    f.spanish_synopsis = "Noviembre de 2019...";
    f.review_count = 642;
    f.genres = {"Ciencia ficcion"};  // must be dropped in favour of IMDb
    f.topics = {"Neo-noir"};
    f.groups = {"Adaptaciones de Philip K. Dick"};
    f.ratings.push_back({"filmaffinity", 8.1, 10.0, std::int64_t{140614}});
    f.source_refs.push_back({"filmaffinity", "358476", std::nullopt});
    return f;
}

}  // namespace

TEST_CASE("merge prefers IMDb for shared fields, keeps FA specialties", "[merge]") {
    const Film m = app::merge_films(imdb_film(), fa_film());

    // IMDb wins the shared fields.
    REQUIRE(m.title == "Blade Runner");
    REQUIRE(m.synopsis == "A blade runner must pursue replicants.");
    REQUIRE(m.genres == std::vector<std::string>{"Sci-Fi", "Action"});

    // FilmAffinity contributes its specialties.
    REQUIRE(m.spanish_title == "Blade Runner");
    REQUIRE(m.spanish_synopsis == "Noviembre de 2019...");
    REQUIRE(m.review_count == 642);
    REQUIRE(m.topics == std::vector<std::string>{"Neo-noir"});
    REQUIRE(m.groups.size() == 1);

    // Both ratings and both source refs survive.
    REQUIRE(m.ratings.size() == 2);
    REQUIRE(m.source_refs.size() == 2);
}

TEST_CASE("merge with a single source passes it through", "[merge]") {
    REQUIRE(app::merge_films(imdb_film(), std::nullopt).title == "Blade Runner");

    const Film only_fa = app::merge_films(std::nullopt, fa_film());
    REQUIRE(only_fa.spanish_title == "Blade Runner");
    REQUIRE(only_fa.title == "Blade Runner");  // FA fallback title
}

TEST_CASE("merge overlays BoxOfficeMojo financials", "[merge]") {
    Film bom;
    bom.budget = 30000000;
    bom.gross = 41600000;
    bom.source_refs.push_back({"boxofficemojo", "tt0083658", std::nullopt});

    const Film m = app::merge_films(imdb_film(), fa_film(), bom);
    REQUIRE(m.title == "Blade Runner");         // IMDb still wins shared fields
    REQUIRE(m.budget == 30000000);
    REQUIRE(m.gross == 41600000);
    REQUIRE(m.source_refs.size() == 3);         // imdb + fa + boxofficemojo

    // BOM alone still contributes its financials.
    const Film only_bom = app::merge_films(std::nullopt, std::nullopt, bom);
    REQUIRE(only_bom.budget == 30000000);
}
