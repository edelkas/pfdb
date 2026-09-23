#include <catch2/catch_test_macros.hpp>

#include "io/field_set.hpp"

using namespace pfdb::io;

TEST_CASE("field tokens round-trip", "[fieldset]") {
    REQUIRE(field_from_token("original-title") == Field::OriginalTitle);
    REQUIRE(field_from_token("Original_Title") == Field::OriginalTitle);  // normalized
    REQUIRE(field_from_token("watch-count") == Field::WatchCount);
    REQUIRE_FALSE(field_from_token("nope").has_value());
    REQUIRE(token_of(Field::ImdbId) == "imdb-id");
}

TEST_CASE("parse_field_list builds a set and rejects unknown tokens", "[fieldset]") {
    const FieldSet s = parse_field_list("title, year ,imdb-id");
    REQUIRE(s.contains(Field::Title));
    REQUIRE(s.contains(Field::Year));
    REQUIRE(s.contains(Field::ImdbId));
    REQUIRE_FALSE(s.contains(Field::Genres));
    // Serialized back in canonical order regardless of input order.
    REQUIRE(s.tokens() == std::vector<std::string>{"title", "year", "imdb-id"});

    REQUIRE_THROWS_AS(parse_field_list("title,bogus"), FieldError);
    REQUIRE_THROWS_AS(parse_field_list(""), FieldError);
}

TEST_CASE("built-in presets split user vs metadata fields", "[fieldset]") {
    const FieldSet all = FieldSet::all();
    const FieldSet user = FieldSet::userdata();
    const FieldSet meta = FieldSet::metadata();

    // userdata: identity anchor + user fields, not the redownloadable metadata.
    REQUIRE(user.contains(Field::ImdbId));
    REQUIRE(user.contains(Field::WatchCount));
    REQUIRE(user.contains(Field::Owned));
    REQUIRE(user.contains(Field::Comments));
    REQUIRE_FALSE(user.contains(Field::Genres));
    REQUIRE_FALSE(user.contains(Field::Cast));

    // metadata: everything except the user fields.
    REQUIRE(meta.contains(Field::Genres));
    REQUIRE(meta.contains(Field::ImdbId));
    REQUIRE_FALSE(meta.contains(Field::WatchCount));
    REQUIRE_FALSE(meta.contains(Field::Favorite));

    REQUIRE(all.contains(Field::Genres));
    REQUIRE(all.contains(Field::WatchCount));

    REQUIRE(is_builtin_preset("all"));
    REQUIRE(is_builtin_preset("userdata"));
    REQUIRE_FALSE(is_builtin_preset("mine"));
}
