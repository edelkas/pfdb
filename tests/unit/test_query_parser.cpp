#include <catch2/catch_test_macros.hpp>

#include <variant>
#include <vector>

#include "query/filter.hpp"
#include "query/parser.hpp"

using namespace pfdb;
using namespace pfdb::query;

namespace {

// Evaluate a parsed expression tree against explicit leaf truth values, so the
// parser's structure (precedence, parens, reductions) can be checked directly.
bool ev(const Expr& e, const std::vector<bool>& v) {
    using K = Expr::Kind;
    switch (e.kind) {
        case K::Const:
            return e.value;
        case K::Leaf:
            return v[e.leaf];
        case K::Not:
            return !ev(*e.kids[0], v);
        case K::And:
            for (const auto& k : e.kids) {
                if (!ev(*k, v)) {
                    return false;
                }
            }
            return true;
        case K::Or:
            for (const auto& k : e.kids) {
                if (ev(*k, v)) {
                    return true;
                }
            }
            return false;
    }
    return false;
}

}  // namespace

TEST_CASE("parse_filter recognizes textual filters", "[query][parser]") {
    const Filter sub = parse_filter("title ~ blade");
    const auto* t = std::get_if<TextFilter>(&sub);
    REQUIRE(t != nullptr);
    REQUIRE(t->field == "title");
    REQUIRE_FALSE(t->regex);
    REQUIRE(t->literal == "blade");

    const Filter re = parse_filter(R"(title =~ blade.+\d+)");
    const auto* tr = std::get_if<TextFilter>(&re);
    REQUIRE(tr != nullptr);
    REQUIRE(tr->regex);
    REQUIRE(tr->re != nullptr);

    // Hyphen/plural aliasing of field names.
    const Filter aliased = parse_filter("original-title ~ x");
    REQUIRE(std::get<TextFilter>(aliased).field == "original_title");
}

TEST_CASE("parse_filter recognizes numeric ranges", "[query][parser]") {
    const auto eq = std::get<NumberFilter>(parse_filter("year = 1982"));
    REQUIRE(eq.min == 1982.0);
    REQUIRE(eq.max == 1982.0);

    const auto range = std::get<NumberFilter>(parse_filter("year = 1940..1949"));
    REQUIRE(range.min == 1940.0);
    REQUIRE(range.max == 1949.0);

    const auto ge = std::get<NumberFilter>(parse_filter("runtime >= 120"));
    REQUIRE(ge.min == 120.0);
    REQUIRE(ge.min_inclusive);
    REQUIRE_FALSE(ge.max.has_value());

    const auto gt = std::get<NumberFilter>(parse_filter("runtime > 120"));
    REQUIRE_FALSE(gt.min_inclusive);

    // Open upper range.
    const auto lo = std::get<NumberFilter>(parse_filter("year = 2000.."));
    REQUIRE(lo.min == 2000.0);
    REQUIRE_FALSE(lo.max.has_value());
}

TEST_CASE("parse_filter recognizes date ranges", "[query][parser]") {
    const auto d = std::get<DateFilter>(parse_filter("date_watched = 2024-01-01..2024-12-31"));
    REQUIRE(d.min == "2024-01-01");
    REQUIRE(d.max == "2024-12-31");

    REQUIRE_THROWS_AS(parse_filter("date_watched = notadate"), QueryError);
}

TEST_CASE("parse_filter recognizes inclusion filters", "[query][parser]") {
    REQUIRE(std::get<StringInFilter>(parse_filter("genre has Sci-Fi")).value == "Sci-Fi");

    const auto name = std::get<NameInclusionFilter>(parse_filter("cast has \"Harrison Ford\""));
    REQUIRE(name.name_field == "cast");
    REQUIRE(name.query == "Harrison Ford");

    REQUIRE(std::get<IdInFilter>(parse_filter("cast_id has 5")).value == 5);
}

TEST_CASE("parse_filter rejects malformed specs", "[query][parser]") {
    REQUIRE_THROWS_AS(parse_filter(""), QueryError);
    REQUIRE_THROWS_AS(parse_filter("nosuchfield ~ x"), QueryError);
    REQUIRE_THROWS_AS(parse_filter("title ? x"), QueryError);
    REQUIRE_THROWS_AS(parse_filter("year ~ x"), QueryError);      // text op on number
    REQUIRE_THROWS_AS(parse_filter("title = 5"), QueryError);     // range op on text
    REQUIRE_THROWS_AS(parse_filter("genre = x"), QueryError);     // range op on list
    REQUIRE_THROWS_AS(parse_filter("year = abc"), QueryError);    // non-number
}

TEST_CASE("parse_where honors precedence and parentheses", "[query][parser]") {
    // NOT > AND > OR.
    const ExprPtr a = parse_where("F1 OR F2 AND F3", 3);
    // F1 OR (F2 AND F3): true when F1, or F2&F3.
    REQUIRE(ev(*a, {true, false, false}));
    REQUIRE_FALSE(ev(*a, {false, true, false}));
    REQUIRE(ev(*a, {false, true, true}));

    const ExprPtr b = parse_where("(F1 OR F2) AND F3", 3);
    REQUIRE_FALSE(ev(*b, {true, false, false}));
    REQUIRE(ev(*b, {true, false, true}));

    const ExprPtr c = parse_where("NOT F1 AND F2", 2);
    REQUIRE(ev(*c, {false, true}));
    REQUIRE_FALSE(ev(*c, {true, true}));
}

TEST_CASE("parse_where accepts symbolic aliases", "[query][parser]") {
    const ExprPtr e = parse_where("!F1 && (F2 || F3)", 3);
    REQUIRE(ev(*e, {false, true, false}));
    REQUIRE_FALSE(ev(*e, {true, true, false}));
}

TEST_CASE("parse_where validates filter references", "[query][parser]") {
    REQUIRE_THROWS_AS(parse_where("F4", 3), QueryError);      // out of range
    REQUIRE_THROWS_AS(parse_where("F1 AND", 1), QueryError);  // dangling operator
    REQUIRE_THROWS_AS(parse_where("(F1", 1), QueryError);     // missing paren
    REQUIRE_THROWS_AS(parse_where("", 1), QueryError);        // empty
}
