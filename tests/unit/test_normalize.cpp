#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "pfdb/film.hpp"
#include "query/eval_context.hpp"
#include "query/expr.hpp"
#include "query/filter.hpp"
#include "query/normalize.hpp"

using namespace pfdb;
using namespace pfdb::query;

namespace {

Film film_with_actor(Id film_id, Id person_id, std::string name) {
    Film f;
    f.id = film_id;
    Credit c;
    c.person = Person{person_id, std::move(name)};
    c.role = CreditRole::Actor;
    f.credits.push_back(c);
    return f;
}

}  // namespace

TEST_CASE("normalize expands a name inclusion into an OR of id filters",
          "[query][normalize]") {
    // Two "John" actors and one unrelated actor.
    PeopleIndex people;
    people["cast"] = {{10, "John Wayne"}, {11, "John Ford"}, {12, "Grace Kelly"}};

    std::vector<Filter> filters{NameInclusionFilter{"cast", "John"}};
    ExprPtr expr = mk_leaf(0);

    ExprPtr norm = normalize(expr, filters, people);

    // The two matching ids became new IdInFilter leaves.
    REQUIRE(filters.size() == 3);  // original + two expansions

    const EvalContext ctx;
    // A film with John Wayne matches; a film with only Grace Kelly does not.
    REQUIRE(eval(*norm, film_with_actor(1, 10, "John Wayne"), ctx, filters));
    REQUIRE(eval(*norm, film_with_actor(2, 11, "John Ford"), ctx, filters));
    REQUIRE_FALSE(eval(*norm, film_with_actor(3, 12, "Grace Kelly"), ctx, filters));
}

TEST_CASE("normalize turns a no-match name into constant-false", "[query][normalize]") {
    PeopleIndex people;
    people["cast"] = {{10, "John Wayne"}};

    std::vector<Filter> filters{NameInclusionFilter{"cast", "Nobody"}};
    ExprPtr norm = normalize(mk_leaf(0), filters, people);

    const EvalContext ctx;
    REQUIRE_FALSE(eval(*norm, film_with_actor(1, 10, "John Wayne"), ctx, filters));
}

TEST_CASE("normalize leaves non-name filters untouched", "[query][normalize]") {
    PeopleIndex people;
    std::vector<Filter> filters{NumberFilter{"year", 2000.0, 2000.0, true, true}};
    ExprPtr norm = normalize(mk_leaf(0), filters, people);
    REQUIRE(filters.size() == 1);
    REQUIRE(norm->kind == Expr::Kind::Leaf);
}
