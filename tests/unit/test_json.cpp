#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "io/film_json.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

TEST_CASE("to_json emits core fields and nulls for unset optionals", "[json]") {
    Film f;
    f.id = 7;
    f.title = "Arrival";
    f.genres = {"Sci-Fi", "Drama"};
    // year left unset -> should serialize as null.

    const nlohmann::json j = to_json(f);

    REQUIRE(j["id"] == 7);
    REQUIRE(j["title"] == "Arrival");
    REQUIRE(j["year"].is_null());
    REQUIRE(j["genres"] == nlohmann::json::array({"Sci-Fi", "Drama"}));
    REQUIRE(j["video"].is_null());
    REQUIRE(j["user"]["favorite"] == false);
}

TEST_CASE("to_json serializes credits, ratings and video", "[json]") {
    Film f;
    f.title = "Blade Runner";
    f.credits.push_back({Person{0, "Ridley Scott"}, CreditRole::Director, "", 0});
    f.ratings.push_back({"imdb", 8.1, 10.0, std::int64_t{800000}});
    VideoFileInfo v;
    v.path = "/m/br.mkv";
    v.codec = "mkv/h265";
    f.video = v;

    const nlohmann::json j = to_json(f);

    REQUIRE(j["credits"][0]["name"] == "Ridley Scott");
    REQUIRE(j["credits"][0]["role"] == "director");
    REQUIRE(j["ratings"][0]["source"] == "imdb");
    REQUIRE(j["ratings"][0]["votes"] == 800000);
    REQUIRE(j["video"]["path"] == "/m/br.mkv");
    REQUIRE(j["video"]["codec"] == "mkv/h265");
}
