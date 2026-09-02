#include "query/engine.hpp"

#include <unordered_map>
#include <unordered_set>

#include "pfdb/credit.hpp"
#include "query/eval_context.hpp"
#include "query/expr.hpp"
#include "query/filter.hpp"
#include "query/parser.hpp"
#include "query/sort.hpp"

namespace pfdb::query {
namespace {

/// The name-inclusion field a credit role feeds, or empty for roles the query
/// engine does not expose by name.
std::string name_field_for_role(CreditRole role) {
    switch (role) {
        case CreditRole::Actor:    return "cast";
        case CreditRole::Director: return "director";
        case CreditRole::Writer:   return "writer";
        default:                   return {};
    }
}

}  // namespace

PeopleIndex build_people_index(const CollectionModel& model) {
    PeopleIndex index;
    std::unordered_map<std::string, std::unordered_set<Id>> seen;
    for (const auto& film : model.all()) {
        for (const auto& credit : film.credits) {
            const std::string field = name_field_for_role(credit.role);
            // A person must have a stored id to be matched by an id filter.
            if (field.empty() || credit.person.id == kInvalidId) {
                continue;
            }
            if (seen[field].insert(credit.person.id).second) {
                index[field].emplace_back(credit.person.id, credit.person.name);
            }
        }
    }
    return index;
}

std::vector<const Film*> run_query(const CollectionModel& model, const QueryRequest& req) {
    std::vector<Filter> filters;
    filters.reserve(req.filter_specs.size());
    for (const auto& spec : req.filter_specs) {
        filters.push_back(parse_filter(spec));
    }

    ExprPtr expr = req.where.empty() ? default_expr(filters.size())
                                     : parse_where(req.where, filters.size());

    const PeopleIndex people = build_people_index(model);
    expr = normalize(expr, filters, people);

    EvalContext ctx;
    ctx.related = &model.related_index();
    ctx.similar = &model.similar_index();

    std::vector<const Film*> out;
    for (const auto& film : model.all()) {
        if (eval(*expr, film, ctx, filters)) {
            out.push_back(&film);
        }
    }

    if (!req.sort.empty()) {
        sort_films(out, parse_sort(req.sort));
    }
    return out;
}

}  // namespace pfdb::query
