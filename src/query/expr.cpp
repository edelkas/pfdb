#include "query/expr.hpp"

namespace pfdb::query {

ExprPtr mk_leaf(std::size_t index) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::Kind::Leaf;
    e->leaf = index;
    return e;
}

ExprPtr mk_const(bool value) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::Kind::Const;
    e->value = value;
    return e;
}

ExprPtr mk_not(ExprPtr a) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::Kind::Not;
    e->kids.push_back(std::move(a));
    return e;
}

ExprPtr mk_and(ExprPtr a, ExprPtr b) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::Kind::And;
    e->kids.push_back(std::move(a));
    e->kids.push_back(std::move(b));
    return e;
}

ExprPtr mk_or(ExprPtr a, ExprPtr b) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::Kind::Or;
    e->kids.push_back(std::move(a));
    e->kids.push_back(std::move(b));
    return e;
}

// a XOR b  ==  (a AND NOT b) OR (NOT a AND b)
ExprPtr mk_xor(const ExprPtr& a, const ExprPtr& b) {
    return mk_or(mk_and(a, mk_not(b)), mk_and(mk_not(a), b));
}

// a XNOR b  ==  (a AND b) OR (NOT a AND NOT b)
ExprPtr mk_xnor(const ExprPtr& a, const ExprPtr& b) {
    return mk_or(mk_and(a, b), mk_and(mk_not(a), mk_not(b)));
}

ExprPtr mk_nand(ExprPtr a, ExprPtr b) { return mk_not(mk_and(std::move(a), std::move(b))); }

ExprPtr mk_nor(ExprPtr a, ExprPtr b) { return mk_not(mk_or(std::move(a), std::move(b))); }

// a -> b  ==  NOT a OR b
ExprPtr mk_imply(ExprPtr a, ExprPtr b) { return mk_or(mk_not(std::move(a)), std::move(b)); }

// a NIMPLY b  ==  a AND NOT b
ExprPtr mk_nimply(ExprPtr a, ExprPtr b) { return mk_and(std::move(a), mk_not(std::move(b))); }

ExprPtr default_expr(std::size_t filter_count) {
    if (filter_count == 0) {
        return mk_const(true);
    }
    ExprPtr acc = mk_leaf(0);
    for (std::size_t i = 1; i < filter_count; ++i) {
        acc = mk_and(std::move(acc), mk_leaf(i));
    }
    return acc;
}

bool eval(const Expr& expr, const Film& film, const EvalContext& ctx,
          const std::vector<Filter>& filters) {
    switch (expr.kind) {
        case Expr::Kind::Const:
            return expr.value;
        case Expr::Kind::Leaf:
            return matches(filters[expr.leaf], film, ctx);
        case Expr::Kind::Not:
            return !eval(*expr.kids[0], film, ctx, filters);
        case Expr::Kind::And:
            for (const auto& k : expr.kids) {
                if (!eval(*k, film, ctx, filters)) {
                    return false;
                }
            }
            return true;
        case Expr::Kind::Or:
            for (const auto& k : expr.kids) {
                if (eval(*k, film, ctx, filters)) {
                    return true;
                }
            }
            return false;
    }
    return false;
}

}  // namespace pfdb::query
