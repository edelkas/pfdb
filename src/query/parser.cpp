#include "query/parser.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "query/field.hpp"
#include "query/text_util.hpp"

namespace pfdb::query {
namespace {

// --------------------------------------------------------------------------
// Small string helpers
// --------------------------------------------------------------------------

std::string_view trim(std::string_view s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string_view unquote(std::string_view s) {
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool is_field_char(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_' || c == '-';
}

/// Parse a full number, rejecting trailing junk. Throws QueryError.
double parse_number(std::string_view text, std::string_view context) {
    const std::string s(trim(text));
    if (s.empty()) {
        throw QueryError("expected a number in '" + std::string(context) + "'");
    }
    try {
        std::size_t consumed = 0;
        const double v = std::stod(s, &consumed);
        if (consumed != s.size()) {
            throw QueryError("not a number: '" + s + "'");
        }
        return v;
    } catch (const std::invalid_argument&) {
        throw QueryError("not a number: '" + s + "'");
    } catch (const std::out_of_range&) {
        throw QueryError("number out of range: '" + s + "'");
    }
}

/// Very light ISO-date shape check: "YYYY-MM-DD".
bool looks_like_date(std::string_view s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(s[i])) == 0) {
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// Filter spec: FIELD OP VALUE
// --------------------------------------------------------------------------

Filter build_range_number(const std::string& field, const std::string& op,
                          std::string_view value) {
    NumberFilter f;
    f.field = field;
    if (op == "=") {
        const std::size_t dots = value.find("..");
        if (dots != std::string_view::npos) {
            const std::string_view lo = trim(value.substr(0, dots));
            const std::string_view hi = trim(value.substr(dots + 2));
            if (!lo.empty()) {
                f.min = parse_number(lo, value);
            }
            if (!hi.empty()) {
                f.max = parse_number(hi, value);
            }
        } else {
            const double v = parse_number(value, value);
            f.min = v;
            f.max = v;
        }
    } else {
        const double v = parse_number(value, value);
        if (op == ">=") {
            f.min = v;
        } else if (op == ">") {
            f.min = v;
            f.min_inclusive = false;
        } else if (op == "<=") {
            f.max = v;
        } else {  // "<"
            f.max = v;
            f.max_inclusive = false;
        }
    }
    return f;
}

std::string require_date(std::string_view value) {
    const std::string s(trim(value));
    if (!looks_like_date(s)) {
        throw QueryError("expected a date YYYY-MM-DD: '" + s + "'");
    }
    return s;
}

Filter build_range_date(const std::string& field, const std::string& op,
                        std::string_view value) {
    DateFilter f;
    f.field = field;
    if (op == "=") {
        const std::size_t dots = value.find("..");
        if (dots != std::string_view::npos) {
            const std::string_view lo = trim(value.substr(0, dots));
            const std::string_view hi = trim(value.substr(dots + 2));
            if (!lo.empty()) {
                f.min = require_date(lo);
            }
            if (!hi.empty()) {
                f.max = require_date(hi);
            }
        } else {
            const std::string v = require_date(value);
            f.min = v;
            f.max = v;
        }
    } else {
        const std::string v = require_date(value);
        if (op == ">=") {
            f.min = v;
        } else if (op == ">") {
            f.min = v;
            f.min_inclusive = false;
        } else if (op == "<=") {
            f.max = v;
        } else {  // "<"
            f.max = v;
            f.max_inclusive = false;
        }
    }
    return f;
}

Filter build_text(const FieldInfo& info, const std::string& op, std::string_view value) {
    TextFilter f;
    f.field = info.name;
    if (op == "=~") {
        f.regex = true;
        try {
            f.re = std::make_shared<std::regex>(
                std::string(value),
                std::regex_constants::ECMAScript | std::regex_constants::icase);
        } catch (const std::regex_error& e) {
            throw QueryError("invalid regex '" + std::string(value) + "': " + e.what());
        }
    } else {  // "~"
        f.literal = std::string(value);
    }
    return f;
}

Filter build_inclusion(const FieldInfo& info, std::string_view value) {
    if (value.empty()) {
        throw QueryError("'has' needs a value for field '" + info.name + "'");
    }
    switch (info.kind) {
        case FieldKind::StringList:
            return StringInFilter{info.name, std::string(value)};
        case FieldKind::NameList:
            return NameInclusionFilter{info.name, std::string(value)};
        case FieldKind::IdList: {
            const double v = parse_number(value, value);
            return IdInFilter{info.name, static_cast<Id>(v)};
        }
        default:
            throw QueryError("field '" + info.name + "' is not a list; 'has' does not apply");
    }
}

void require_kind(const FieldInfo& info, FieldKind a, FieldKind b, const std::string& op) {
    if (info.kind != a && info.kind != b) {
        throw QueryError("operator '" + op + "' does not apply to field '" + info.name + "'");
    }
}

}  // namespace

Filter parse_filter(std::string_view spec) {
    const std::string_view s = trim(spec);
    if (s.empty()) {
        throw QueryError("empty filter");
    }

    // Field token.
    std::size_t p = 0;
    while (p < s.size() && is_field_char(s[p])) {
        ++p;
    }
    if (p == 0) {
        throw QueryError("filter must start with a field: '" + std::string(s) + "'");
    }
    const std::string_view field_tok = s.substr(0, p);

    // Whitespace, then operator.
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) {
        ++p;
    }
    std::string op;
    const std::string_view rest = s.substr(p);
    for (const char* cand : {"=~", ">=", "<=", "~", "=", ">", "<"}) {
        if (rest.substr(0, std::string_view(cand).size()) == cand) {
            op = cand;
            p += op.size();
            break;
        }
    }
    if (op.empty()) {
        // Word operator ("has").
        std::size_t w = p;
        while (w < s.size() && std::isalpha(static_cast<unsigned char>(s[w])) != 0) {
            ++w;
        }
        const std::string_view word = s.substr(p, w - p);
        if (!ci_equals(word, "has")) {
            throw QueryError("expected an operator (~, =~, =, <, >, <=, >=, has) in '" +
                             std::string(s) + "'");
        }
        op = "has";
        p = w;
    }

    // Value (everything left, trimmed and unquoted).
    const std::string_view value = unquote(trim(s.substr(p)));

    const auto info = find_field(field_tok);
    if (!info) {
        throw QueryError("unknown field: '" + std::string(field_tok) + "'");
    }

    if (op == "~" || op == "=~") {
        require_kind(*info, FieldKind::Text, FieldKind::Text, op);
        return build_text(*info, op, value);
    }
    if (op == "has") {
        return build_inclusion(*info, value);
    }
    // Comparison / range operator.
    require_kind(*info, FieldKind::Number, FieldKind::Date, op);
    if (info->kind == FieldKind::Number) {
        return build_range_number(info->name, op, value);
    }
    return build_range_date(info->name, op, value);
}

// --------------------------------------------------------------------------
// Boolean expression over F1..Fn
// --------------------------------------------------------------------------

namespace {

enum class TokType {
    LParen, RParen, Not, And, Or, Nand, Nor, Xor, Xnor, Imply, Nimply, Ident, End
};

struct Tok {
    TokType type = TokType::End;
    std::size_t leaf = 0;
};

bool is_binary(TokType t) {
    switch (t) {
        case TokType::And:
        case TokType::Or:
        case TokType::Nand:
        case TokType::Nor:
        case TokType::Xor:
        case TokType::Xnor:
        case TokType::Imply:
        case TokType::Nimply:
            return true;
        default:
            return false;
    }
}

int precedence(TokType t) {
    switch (t) {
        case TokType::And:
        case TokType::Nand:
            return 3;
        case TokType::Xor:
        case TokType::Xnor:
            return 2;
        case TokType::Or:
        case TokType::Nor:
            return 1;
        case TokType::Imply:
        case TokType::Nimply:
            return 0;
        default:
            return -1;
    }
}

bool right_assoc(TokType t) { return t == TokType::Imply || t == TokType::Nimply; }

ExprPtr build_binary(TokType t, ExprPtr a, ExprPtr b) {
    switch (t) {
        case TokType::And:    return mk_and(std::move(a), std::move(b));
        case TokType::Or:     return mk_or(std::move(a), std::move(b));
        case TokType::Nand:   return mk_nand(std::move(a), std::move(b));
        case TokType::Nor:    return mk_nor(std::move(a), std::move(b));
        case TokType::Xor:    return mk_xor(a, b);
        case TokType::Xnor:   return mk_xnor(a, b);
        case TokType::Imply:  return mk_imply(std::move(a), std::move(b));
        case TokType::Nimply: return mk_nimply(std::move(a), std::move(b));
        default:              return mk_const(false);  // unreachable
    }
}

std::vector<Tok> tokenize_where(std::string_view expr, std::size_t filter_count) {
    std::vector<Tok> toks;
    std::size_t i = 0;
    while (i < expr.size()) {
        const char c = expr[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (c == '(') {
            toks.push_back({TokType::LParen, 0});
            ++i;
        } else if (c == ')') {
            toks.push_back({TokType::RParen, 0});
            ++i;
        } else if (c == '!') {
            toks.push_back({TokType::Not, 0});
            ++i;
        } else if (c == '&') {
            toks.push_back({TokType::And, 0});
            i += (i + 1 < expr.size() && expr[i + 1] == '&') ? 2 : 1;
        } else if (c == '|') {
            toks.push_back({TokType::Or, 0});
            i += (i + 1 < expr.size() && expr[i + 1] == '|') ? 2 : 1;
        } else if ((std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_') {
            std::size_t j = i;
            while (j < expr.size() &&
                   ((std::isalnum(static_cast<unsigned char>(expr[j])) != 0) ||
                    expr[j] == '_')) {
                ++j;
            }
            const std::string_view word = expr.substr(i, j - i);
            const std::string lw = to_lower_ascii(word);
            i = j;
            if (lw == "and") {
                toks.push_back({TokType::And, 0});
            } else if (lw == "or") {
                toks.push_back({TokType::Or, 0});
            } else if (lw == "not") {
                toks.push_back({TokType::Not, 0});
            } else if (lw == "nand") {
                toks.push_back({TokType::Nand, 0});
            } else if (lw == "nor") {
                toks.push_back({TokType::Nor, 0});
            } else if (lw == "xor") {
                toks.push_back({TokType::Xor, 0});
            } else if (lw == "xnor" || lw == "equiv") {
                toks.push_back({TokType::Xnor, 0});
            } else if (lw == "imply") {
                toks.push_back({TokType::Imply, 0});
            } else if (lw == "nimply") {
                toks.push_back({TokType::Nimply, 0});
            } else if ((lw[0] == 'f') && lw.size() > 1) {
                // Filter reference F<number>.
                const std::string digits = lw.substr(1);
                if (digits.find_first_not_of("0123456789") != std::string::npos) {
                    throw QueryError("bad filter reference: '" + std::string(word) + "'");
                }
                const long n = std::stol(digits);
                if (n < 1 || static_cast<std::size_t>(n) > filter_count) {
                    throw QueryError("filter reference out of range: '" + std::string(word) +
                                     "' (have " + std::to_string(filter_count) + ")");
                }
                toks.push_back({TokType::Ident, static_cast<std::size_t>(n - 1)});
            } else {
                throw QueryError("unexpected token in expression: '" + std::string(word) +
                                 "'");
            }
        } else {
            throw QueryError(std::string("unexpected character in expression: '") + c + "'");
        }
    }
    toks.push_back({TokType::End, 0});
    return toks;
}

class WhereParser {
public:
    explicit WhereParser(std::vector<Tok> toks) : toks_(std::move(toks)) {}

    ExprPtr parse() {
        ExprPtr e = parse_expr(0);
        if (peek().type != TokType::End) {
            throw QueryError("trailing tokens in expression");
        }
        return e;
    }

private:
    const Tok& peek() const { return toks_[pos_]; }
    const Tok& next() { return toks_[pos_++]; }

    ExprPtr parse_expr(int min_prec) {
        ExprPtr left = parse_unary();
        while (is_binary(peek().type) && precedence(peek().type) >= min_prec) {
            const TokType op = next().type;
            const int next_min = right_assoc(op) ? precedence(op) : precedence(op) + 1;
            ExprPtr right = parse_expr(next_min);
            left = build_binary(op, std::move(left), std::move(right));
        }
        return left;
    }

    ExprPtr parse_unary() {
        if (peek().type == TokType::Not) {
            next();
            return mk_not(parse_unary());
        }
        return parse_primary();
    }

    ExprPtr parse_primary() {
        const Tok& t = peek();
        if (t.type == TokType::LParen) {
            next();
            ExprPtr e = parse_expr(0);
            if (peek().type != TokType::RParen) {
                throw QueryError("missing ')' in expression");
            }
            next();
            return e;
        }
        if (t.type == TokType::Ident) {
            const std::size_t leaf = next().leaf;
            return mk_leaf(leaf);
        }
        throw QueryError("expected a filter reference or '(' in expression");
    }

    std::vector<Tok> toks_;
    std::size_t pos_ = 0;
};

}  // namespace

ExprPtr parse_where(std::string_view expr, std::size_t filter_count) {
    const std::string_view s = trim(expr);
    if (s.empty()) {
        throw QueryError("empty boolean expression");
    }
    WhereParser parser(tokenize_where(s, filter_count));
    return parser.parse();
}

}  // namespace pfdb::query
