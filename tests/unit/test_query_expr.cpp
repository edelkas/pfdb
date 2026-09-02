#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "query/expr.hpp"

using namespace pfdb::query;

namespace {

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

// Check a two-input connective against its truth table {f(0,0),f(0,1),f(1,0),f(1,1)}.
void check_binary(const ExprPtr& e, bool ff, bool ft, bool tf, bool tt) {
    REQUIRE(ev(*e, {false, false}) == ff);
    REQUIRE(ev(*e, {false, true}) == ft);
    REQUIRE(ev(*e, {true, false}) == tf);
    REQUIRE(ev(*e, {true, true}) == tt);
}

}  // namespace

TEST_CASE("extended operators reduce to correct truth tables", "[query][expr]") {
    const ExprPtr a = mk_leaf(0);
    const ExprPtr b = mk_leaf(1);

    check_binary(mk_and(a, b), false, false, false, true);
    check_binary(mk_or(a, b), false, true, true, true);
    check_binary(mk_xor(a, b), false, true, true, false);
    check_binary(mk_xnor(a, b), true, false, false, true);
    check_binary(mk_nand(a, b), true, true, true, false);
    check_binary(mk_nor(a, b), true, false, false, false);
    check_binary(mk_imply(a, b), true, true, false, true);   // a -> b
    check_binary(mk_nimply(a, b), false, false, true, false);  // a AND NOT b
}

TEST_CASE("default_expr is the AND of all filters", "[query][expr]") {
    const ExprPtr all = default_expr(3);
    REQUIRE(ev(*all, {true, true, true}));
    REQUIRE_FALSE(ev(*all, {true, false, true}));

    // With no filters, everything passes.
    REQUIRE(ev(*default_expr(0), {}));
}
