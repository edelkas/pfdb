#include <catch2/catch_test_macros.hpp>

#include "pfdb/credit.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

TEST_CASE("credit role tokens round-trip", "[domain]") {
    const CreditRole roles[] = {
        CreditRole::Director,     CreditRole::Writer,   CreditRole::Actor,
        CreditRole::Producer,     CreditRole::Composer, CreditRole::Cinematographer,
        CreditRole::Editor,       CreditRole::Other,
    };
    for (CreditRole role : roles) {
        REQUIRE(credit_role_from_string(to_string(role)) == role);
    }
}

TEST_CASE("unknown credit role token maps to Other", "[domain]") {
    REQUIRE(credit_role_from_string("gaffer") == CreditRole::Other);
    REQUIRE(credit_role_from_string("") == CreditRole::Other);
}

TEST_CASE("Film has value semantics", "[domain]") {
    Film a;
    a.title = "Blade Runner";
    a.year = 1982;
    a.genres = {"Sci-Fi", "Thriller"};
    a.credits.push_back({Person{0, "Ridley Scott"}, CreditRole::Director, "", 0});

    Film b = a;
    REQUIRE(a == b);

    b.year = 1983;
    REQUIRE_FALSE(a == b);
}
