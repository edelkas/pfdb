#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "pfdb/film.hpp"
#include "query/eval_context.hpp"
#include "query/filter.hpp"

namespace pfdb::query {

/// A node in the boolean filter expression. Only the three basic connectives
/// plus leaves and constants ever exist at evaluation time: every extended
/// operator (XOR, NAND, IMPLY, ...) is reduced to these when the tree is built.
struct Expr {
    enum class Kind { And, Or, Not, Leaf, Const };

    Kind kind = Kind::Const;
    std::vector<std::shared_ptr<Expr>> kids;  ///< And/Or: 1+; Not: exactly 1.
    std::size_t leaf = 0;                      ///< Index into the filter list (Leaf).
    bool value = false;                        ///< Constant truth value (Const).
};

using ExprPtr = std::shared_ptr<Expr>;

// --- Builders (used by the parser and by normalization) ---
ExprPtr mk_leaf(std::size_t index);
ExprPtr mk_const(bool value);
ExprPtr mk_not(ExprPtr a);
ExprPtr mk_and(ExprPtr a, ExprPtr b);
ExprPtr mk_or(ExprPtr a, ExprPtr b);

// Extended connectives, each reduced to And/Or/Not immediately.
ExprPtr mk_xor(const ExprPtr& a, const ExprPtr& b);
ExprPtr mk_xnor(const ExprPtr& a, const ExprPtr& b);  // a == b  (also EQUIV)
ExprPtr mk_nand(ExprPtr a, ExprPtr b);
ExprPtr mk_nor(ExprPtr a, ExprPtr b);
ExprPtr mk_imply(ExprPtr a, ExprPtr b);  // a -> b
ExprPtr mk_nimply(ExprPtr a, ExprPtr b); // a AND NOT b

/// The default expression when the user gives no `--where`: the AND of every
/// filter (all must hold). With no filters this is the constant true.
ExprPtr default_expr(std::size_t filter_count);

/// Evaluate `expr` against `film`, resolving leaves through `filters`.
/// Short-circuits And/Or.
bool eval(const Expr& expr, const Film& film, const EvalContext& ctx,
          const std::vector<Filter>& filters);

}  // namespace pfdb::query
