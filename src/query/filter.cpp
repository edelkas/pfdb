#include "query/filter.hpp"

#include "query/field.hpp"
#include "query/text_util.hpp"

namespace pfdb::query {
namespace {

bool below_min(double v, const NumberFilter& f) {
    return f.min && (f.min_inclusive ? v < *f.min : v <= *f.min);
}
bool above_max(double v, const NumberFilter& f) {
    return f.max && (f.max_inclusive ? v > *f.max : v >= *f.max);
}
bool below_min(const std::string& v, const DateFilter& f) {
    return f.min && (f.min_inclusive ? v < *f.min : v <= *f.min);
}
bool above_max(const std::string& v, const DateFilter& f) {
    return f.max && (f.max_inclusive ? v > *f.max : v >= *f.max);
}

}  // namespace

bool matches(const Filter& filter, const Film& film, const EvalContext& ctx) {
    if (const auto* t = std::get_if<TextFilter>(&filter)) {
        const auto val = extract_text(t->field, film);
        if (!val) {
            return false;
        }
        if (t->regex) {
            return t->re && std::regex_search(*val, *t->re);
        }
        return ci_contains(*val, t->literal);
    }
    if (const auto* n = std::get_if<NumberFilter>(&filter)) {
        const auto val = extract_number(n->field, film);
        return val && !below_min(*val, *n) && !above_max(*val, *n);
    }
    if (const auto* d = std::get_if<DateFilter>(&filter)) {
        const auto val = extract_date(d->field, film);
        return val && !below_min(*val, *d) && !above_max(*val, *d);
    }
    if (const auto* s = std::get_if<StringInFilter>(&filter)) {
        for (const auto& x : extract_strings(s->field, film)) {
            if (ci_equals(x, s->value)) {
                return true;
            }
        }
        return false;
    }
    if (const auto* i = std::get_if<IdInFilter>(&filter)) {
        for (Id x : extract_ids(i->field, film, ctx)) {
            if (x == i->value) {
                return true;
            }
        }
        return false;
    }
    // NameInclusionFilter is expanded during normalization and should never be
    // evaluated directly; if one survives, it matches nothing.
    return false;
}

}  // namespace pfdb::query
