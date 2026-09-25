#include <catch2/catch_test_macros.hpp>

#include "sources/boxofficemojo/bom_parser.hpp"
#include "support/fixtures.hpp"

using namespace pfdb::sources::boxofficemojo;

TEST_CASE("bom parser extracts budget and worldwide gross", "[bom]") {
    const auto html = pfdb::test::read_fixture("bom/title_tt1856101.html");
    const Financials f = parse_financials(html);
    REQUIRE(f.budget == 150000000);
    REQUIRE(f.gross == 277882781);
}

TEST_CASE("bom parser tolerates a page with no figures", "[bom]") {
    const Financials f = parse_financials("<html><body><p>Nothing here</p></body></html>");
    REQUIRE_FALSE(f.budget.has_value());
    REQUIRE_FALSE(f.gross.has_value());
}
