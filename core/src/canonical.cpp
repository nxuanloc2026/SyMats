// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Canonical constructors for Plus, Times and Power (docs/EXPR_SPEC.md §2).
#include <algorithm>
#include <map>

#include "symats/expr.h"

namespace symats {

namespace {

// Exponents above this are left unevaluated to avoid enormous exact numbers.
constexpr long long kMaxExactExponent = 100000;

const ExprPtr& sym_plus() {
    static const ExprPtr s = make_symbol("Plus");
    return s;
}
const ExprPtr& sym_times() {
    static const ExprPtr s = make_symbol("Times");
    return s;
}
const ExprPtr& sym_power() {
    static const ExprPtr s = make_symbol("Power");
    return s;
}

ExprPtr raw_power(const ExprPtr& b, const ExprPtr& e) { return make_normal(sym_power(), {b, e}); }

// Splits a term into (numeric coefficient, rest):  Times[3, x, y] -> (3, Times[x, y]).
std::pair<Rational, ExprPtr> split_coefficient(const ExprPtr& t) {
    if (t->has_head("Times") && t->size() >= 2 && t->arg(0)->is_number()) {
        Rational c = t->arg(0)->number();
        if (t->size() == 2) return {c, t->arg(1)};
        ExprList rest(t->args().begin() + 1, t->args().end());
        return {c, make_normal(sym_times(), std::move(rest))};
    }
    return {Rational(1), t};
}

// Inverse of split_coefficient. `rest` is already canonical and has no numeric factor.
ExprPtr with_coefficient(const Rational& c, const ExprPtr& rest) {
    if (c.is_one()) return rest;
    ExprList f{make_number(c)};
    if (rest->has_head("Times")) f.insert(f.end(), rest->args().begin(), rest->args().end());
    else f.push_back(rest);
    return make_normal(sym_times(), std::move(f));
}

void collect_terms(const ExprPtr& t, Rational& number,
                   std::map<ExprPtr, Rational, ExprLess>& coeffs) {
    if (t->has_head("Plus")) {
        for (const auto& a : t->args()) collect_terms(a, number, coeffs);
        return;
    }
    if (t->is_number()) {
        number = number + t->number();
        return;
    }
    // Inside a sum, a numeric multiple of a sum is distributed:
    // a + 2*(b + c) -> a + 2*b + 2*c. This makes (x + 1) - (x + 1) cancel to 0.
    // It is done here, not in times(), so that times() stays independent of grouping:
    // (2*(b + c))*d == 2*(b + c)*d.
    if (t->has_head("Times") && t->size() == 2 && t->arg(0)->is_number() &&
        t->arg(1)->has_head("Plus")) {
        const ExprPtr& c = t->arg(0);
        for (const auto& term : t->arg(1)->args()) collect_terms(times(c, term), number, coeffs);
        return;
    }
    auto [c, rest] = split_coefficient(t);
    auto it = coeffs.find(rest);
    if (it == coeffs.end()) coeffs.emplace(rest, c);
    else it->second = it->second + c;
}

void collect_factors(const ExprPtr& f, Rational& coeff,
                     std::map<ExprPtr, ExprList, ExprLess>& exponents) {
    if (f->has_head("Times")) {
        for (const auto& a : f->args()) collect_factors(a, coeff, exponents);
        return;
    }
    if (f->is_number()) {
        coeff = coeff * f->number();
        return;
    }
    if (f->has_head("Power") && f->size() == 2) {
        exponents[f->arg(0)].push_back(f->arg(1));
        return;
    }
    exponents[f].push_back(make_integer(1));
}

}  // namespace

// ---------------------------------------------------------------- Plus

ExprPtr plus(ExprList terms) {
    Rational number = 0;
    std::map<ExprPtr, Rational, ExprLess> coeffs;  // ordered by term, ignoring coefficient
    for (const auto& t : terms) collect_terms(t, number, coeffs);

    ExprList out;
    if (!number.is_zero()) out.push_back(make_number(number));
    for (const auto& [rest, c] : coeffs)
        if (!c.is_zero()) out.push_back(with_coefficient(c, rest));

    if (out.empty()) return make_integer(0);
    if (out.size() == 1) return out[0];
    return make_normal(sym_plus(), std::move(out));
}

ExprPtr plus(const ExprPtr& a, const ExprPtr& b) { return plus(ExprList{a, b}); }

// ---------------------------------------------------------------- Times

ExprPtr times(ExprList factors) {
    Rational coeff = 1;
    std::map<ExprPtr, ExprList, ExprLess> exponents;  // base -> exponents to add
    for (const auto& f : factors) collect_factors(f, coeff, exponents);
    if (coeff.is_zero()) return make_integer(0);

    ExprList out;
    bool needs_second_pass = false;
    for (const auto& [base, exps] : exponents) {
        ExprPtr p = power(base, plus(exps));
        if (p->is_number()) {
            coeff = coeff * p->number();
        } else {
            if (p->has_head("Times")) needs_second_pass = true;
            out.push_back(p);
        }
    }
    if (coeff.is_zero()) return make_integer(0);

    if (needs_second_pass) {
        // A power expanded into a product (e.g. Power[Times[2, x], 1] -> Times[2, x]):
        // collect again so equal bases merge.
        out.push_back(make_number(coeff));
        return times(std::move(out));
    }

    std::sort(out.begin(), out.end(), ExprLess{});
    if (!coeff.is_one()) out.insert(out.begin(), make_number(coeff));
    if (out.empty()) return make_integer(1);
    if (out.size() == 1) return out[0];
    return make_normal(sym_times(), std::move(out));
}

ExprPtr times(const ExprPtr& a, const ExprPtr& b) { return times(ExprList{a, b}); }

// ---------------------------------------------------------------- Power

ExprPtr power(const ExprPtr& b, const ExprPtr& e) {
    if (e->is_number()) {
        const Rational r = e->number();
        if (r.is_zero()) {
            if (b->is_number() && b->number().is_zero()) return make_symbol("Indeterminate");
            return make_integer(1);
        }
        if (r.is_one()) return b;
    }

    if (b->is_number()) {
        const Rational base = b->number();
        if (base.is_one()) return make_integer(1);
        if (e->is_number()) {
            const Rational r = e->number();
            if (base.is_zero())
                return r.sign() > 0 ? make_integer(0) : make_symbol("ComplexInfinity");

            if (r.is_integer()) {
                auto n = r.num().to_int64();
                if (n && *n <= kMaxExactExponent && *n >= -kMaxExactExponent)
                    return make_number(Rational::pow(base, *n));
                return raw_power(b, e);
            }
            // Fractional exponent p/q: simplify only when an exact root exists.
            // Negative bases are left alone: the principal value is complex.
            if (base.sign() > 0) {
                auto p = r.num().to_int64();
                auto q = r.den().to_int64();
                if (p && q && *p <= kMaxExactExponent && *p >= -kMaxExactExponent) {
                    if (auto root = base.exact_root(static_cast<unsigned long long>(*q)))
                        return make_number(Rational::pow(*root, *p));
                }
            }
        }
        return raw_power(b, e);
    }

    // (x^a)^n = x^(a*n) and (x*y)^n = x^n * y^n are valid for integer n.
    if (e->is_integer()) {
        if (b->has_head("Power") && b->size() == 2)
            return power(b->arg(0), times(b->arg(1), e));
        if (b->has_head("Times")) {
            ExprList f;
            f.reserve(b->size());
            for (const auto& a : b->args()) f.push_back(power(a, e));
            return times(std::move(f));
        }
    }

    return raw_power(b, e);
}

// ---------------------------------------------------------------- helpers

ExprPtr negate(const ExprPtr& a) { return times(make_integer(-1), a); }
ExprPtr subtract(const ExprPtr& a, const ExprPtr& b) { return plus(a, negate(b)); }
ExprPtr divide(const ExprPtr& a, const ExprPtr& b) { return times(a, power(b, make_integer(-1))); }

}  // namespace symats
