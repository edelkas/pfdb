#include "query/normalize.hpp"

#include <unordered_map>

#include "query/field.hpp"
#include "query/text_util.hpp"

namespace pfdb::query {
namespace {

/// Expand a name-based inclusion into an OR of exact id filters. Appends the new
/// id filters to `filters` and returns the resulting expression (constant-false
/// when the name matches nobody). `nf` is taken by value because appending to
/// `filters` may reallocate the vector it was read from.
ExprPtr expand_name(NameInclusionFilter nf, std::vector<Filter>& filters,
                    const PeopleIndex& people) {
    const std::string id_field = id_field_for(nf.name_field);
    auto it = people.find(nf.name_field);
    ExprPtr result;
    if (it != people.end()) {
        for (const auto& [id, name] : it->second) {
            if (ci_contains(name, nf.query)) {
                const std::size_t idx = filters.size();
                filters.push_back(IdInFilter{id_field, id});
                ExprPtr leaf = mk_leaf(idx);
                result = result ? mk_or(std::move(result), std::move(leaf)) : std::move(leaf);
            }
        }
    }
    return result ? result : mk_const(false);
}

ExprPtr walk(const ExprPtr& expr, std::vector<Filter>& filters, const PeopleIndex& people,
             std::unordered_map<std::size_t, ExprPtr>& expanded) {
    switch (expr->kind) {
        case Expr::Kind::Const:
            return mk_const(expr->value);
        case Expr::Kind::Leaf: {
            const Filter& f = filters[expr->leaf];
            if (const auto* nf = std::get_if<NameInclusionFilter>(&f)) {
                // Expand once per original leaf so a shared sub-tree (e.g. from
                // an XOR reduction) does not append duplicate id filters.
                auto cached = expanded.find(expr->leaf);
                if (cached != expanded.end()) {
                    return cached->second;
                }
                ExprPtr e = expand_name(*nf, filters, people);
                expanded.emplace(expr->leaf, e);
                return e;
            }
            return mk_leaf(expr->leaf);
        }
        case Expr::Kind::Not:
            return mk_not(walk(expr->kids[0], filters, people, expanded));
        case Expr::Kind::And:
        case Expr::Kind::Or: {
            auto out = std::make_shared<Expr>();
            out->kind = expr->kind;
            for (const auto& k : expr->kids) {
                out->kids.push_back(walk(k, filters, people, expanded));
            }
            return out;
        }
    }
    return mk_const(false);
}

}  // namespace

ExprPtr normalize(const ExprPtr& expr, std::vector<Filter>& filters,
                  const PeopleIndex& people) {
    std::unordered_map<std::size_t, ExprPtr> expanded;
    return walk(expr, filters, people, expanded);
}

}  // namespace pfdb::query
