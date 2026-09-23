#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "io/emdb/emdb_decode.hpp"
#include "io/emdb/emdb_map.hpp"
#include "io/emdb/emdb_parser.hpp"
#include "io/field_set.hpp"
#include "pfdb/film.hpp"
#include "support/fixtures.hpp"

using namespace pfdb;
using namespace pfdb::io::emdb;

namespace {

EmdbData load_sample() {
    return parse(bytes_to_json(pfdb::test::read_fixture("emdb/sample.dat")));
}

bool has_genre(const Film& f, const std::string& g) {
    return std::find(f.genres.begin(), f.genres.end(), g) != f.genres.end();
}

const Credit* credit_of(const Film& f, CreditRole role) {
    for (const auto& c : f.credits) {
        if (c.role == role) {
            return &c;
        }
    }
    return nullptr;
}

std::string imdb_id(const Film& f) {
    for (const auto& r : f.source_refs) {
        if (r.source == "imdb") {
            return r.external_id;
        }
    }
    return {};
}

}  // namespace

TEST_CASE("emdb map fills every field under the 'all' preset", "[emdb][map]") {
    const auto films = to_films(load_sample(), io::FieldSet::all());
    REQUIRE(films.size() == 2);
    const Film& f = films[0];

    REQUIRE(f.title == "Blade Runner");
    REQUIRE(f.original_title == "Blade Runner: The Final Cut");
    REQUIRE(f.year == 1982);
    REQUIRE(f.runtime_minutes == 117);
    REQUIRE(f.synopsis.rfind("A blade runner", 0) == 0);
    REQUIRE(has_genre(f, "Sci-Fi"));
    REQUIRE(has_genre(f, "Giallo"));  // custom genre code '1'

    const Credit* actor = credit_of(f, CreditRole::Actor);
    REQUIRE(actor != nullptr);
    REQUIRE(actor->person.name == "Harrison Ford");
    REQUIRE(actor->character == "Rick Deckard");
    REQUIRE(credit_of(f, CreditRole::Director)->person.name == "Ridley Scott");
    REQUIRE(credit_of(f, CreditRole::Writer)->person.name == "Hampton Fancher");
    REQUIRE(credit_of(f, CreditRole::Composer)->person.name == "Vangelis");

    REQUIRE(imdb_id(f) == "tt0083658");
    REQUIRE(f.ratings.size() == 1);
    REQUIRE(f.ratings[0].value == 8.1);       // 0x51 = 81 -> 8.1
    REQUIRE(f.ratings[0].votes == 263000);

    // Personal bitmask 36 (owned bit2 + favorite bit5) + play count 3.
    REQUIRE(f.user.watch_count == 3);
    REQUIRE(f.user.owned);
    REQUIRE(f.user.favorite);
    REQUIRE_FALSE(f.user.wishlist);
    REQUIRE(f.user.date_watched == "2024-01-15");
    REQUIRE(f.user.personal_rating == 2.5);   // 0x19 = 25 -> 2.5
    REQUIRE(f.user.notes == "Great \"tears in rain\" monologue.");

    REQUIRE(f.topics == std::vector<std::string>{"Cyberpunk", "Dystopia"});
    REQUIRE(f.groups == std::vector<std::string>{"Blade Runner Collection"});

    REQUIRE(f.video.has_value());
    REQUIRE(f.video->codec == "H.265");
    REQUIRE(f.video->width == 1920);
    REQUIRE(f.video->height == 1080);
    REQUIRE(f.video->size_bytes == 1500LL * 1024 * 1024);
}

TEST_CASE("emdb map under 'userdata' keeps only the user fields + anchor",
          "[emdb][map]") {
    const auto films = to_films(load_sample(), io::FieldSet::userdata());
    const Film& f = films[0];

    // Kept: identity anchor + display + user data.
    REQUIRE(f.title == "Blade Runner");
    REQUIRE(f.year == 1982);
    REQUIRE(imdb_id(f) == "tt0083658");
    REQUIRE(f.user.watch_count == 3);
    REQUIRE(f.user.owned);

    // Dropped: metadata to be redownloaded.
    REQUIRE(f.genres.empty());
    REQUIRE(f.credits.empty());
    REQUIRE(f.ratings.empty());
    REQUIRE(f.topics.empty());
    REQUIRE_FALSE(f.video.has_value());
    REQUIRE(f.synopsis.empty());
}
