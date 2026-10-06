// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace symats {
namespace {

bool contains_var(const ExprPtr& e, const ExprPtr& x) {
    if (!e || !x) return false;
    if (equal(e, x)) return true;
    if (e->is_number()) return false;
    if (e->is_symbol()) return false;
    if (e->is_normal()) {
        if (contains_var(e->head(), x)) return true;
        for (const auto& a : e->args()) {
            if (contains_var(a, x)) return true;
        }
    }
    return false;
}

ExprPtr diff_impl(const ExprPtr& f, const ExprPtr& x) {
    if (!f || !x) return make_integer(0);

    // D[f, x] = 1 if f == x
    if (equal(f, x)) return make_integer(1);

    // Constant w.r.t x -> 0
    if (!contains_var(f, x)) return make_integer(0);

    // Number -> 0
    if (f->is_number()) return make_integer(0);

    // Symbol != x -> 0
    if (f->is_symbol()) return make_integer(0);

    if (f->is_normal()) {
        const auto& head = f->head();

        if (f->has_head("Plus")) {
            ExprList terms;
            terms.reserve(f->size());
            for (const auto& a : f->args()) {
                terms.push_back(diff_impl(a, x));
            }
            return plus(std::move(terms));
        }

        if (f->has_head("Times")) {
            // Product rule: sum_i (d(a_i)/dx * prod_{j != i} a_j)
            ExprList terms;
            terms.reserve(f->size());
            for (std::size_t i = 0; i < f->size(); ++i) {
                ExprList factors;
                factors.reserve(f->size());
                for (std::size_t j = 0; j < f->size(); ++j) {
                    if (i == j) {
                        factors.push_back(diff_impl(f->arg(j), x));
                    } else {
                        factors.push_back(f->arg(j));
                    }
                }
                terms.push_back(times(std::move(factors)));
            }
            return plus(std::move(terms));
        }

        if (f->has_head("Power") && f->size() == 2) {
            const auto& base = f->arg(0);
            const auto& exp = f->arg(1);
            const bool base_has = contains_var(base, x);
            const bool exp_has = contains_var(exp, x);

            if (base_has && !exp_has) {
                // d/dx [u^n] = n * u^(n-1) * u'
                ExprPtr exp_minus_1 = plus({exp, make_integer(-1)});
                return times({exp, power(base, exp_minus_1), diff_impl(base, x)});
            }
            if (!base_has && exp_has) {
                // d/dx [a^v] = a^v * Log[a] * v'
                return times({f, make_normal("Log", {base}), diff_impl(exp, x)});
            }
            if (base_has && exp_has) {
                // d/dx [u^v] = u^v * (v' * Log[u] + v * u' / u)
                ExprPtr term1 = times({diff_impl(exp, x), make_normal("Log", {base})});
                ExprPtr term2 = divide(times({exp, diff_impl(base, x)}), base);
                return times({f, plus({term1, term2})});
            }
            return make_integer(0);
        }

        if (head->is_symbol()) {
            std::string_view hname = head->name();
            if (f->size() == 1) {
                const auto& u = f->arg(0);
                ExprPtr du = diff_impl(u, x);

                if (hname == "Sin") return times({make_normal("Cos", {u}), du});
                if (hname == "Cos") return times({make_integer(-1), make_normal("Sin", {u}), du});
                if (hname == "Tan") {
                    ExprPtr sec_u = power(make_normal("Cos", {u}), make_integer(-2));
                    return times({sec_u, du});
                }
                if (hname == "Exp") return times({f, du});
                if (hname == "Log") return divide(du, u);
                if (hname == "ArcSin") {
                    ExprPtr denom = power(plus({make_integer(1), negate(power(u, make_integer(2)))}), make_rational(1, 2));
                    return divide(du, denom);
                }
                if (hname == "ArcCos") {
                    ExprPtr denom = power(plus({make_integer(1), negate(power(u, make_integer(2)))}), make_rational(1, 2));
                    return divide(negate(du), denom);
                }
                if (hname == "ArcTan") {
                    ExprPtr denom = plus({make_integer(1), power(u, make_integer(2))});
                    return divide(du, denom);
                }
                if (hname == "Sinh") return times({make_normal("Cosh", {u}), du});
                if (hname == "Cosh") return times({make_normal("Sinh", {u}), du});
                if (hname == "Tanh") {
                    ExprPtr sech2 = power(make_normal("Cosh", {u}), make_integer(-2));
                    return times({sech2, du});
                }
                if (hname == "Abs") {
                    return divide(times({u, du}), make_normal("Abs", {u}));
                }

                // Generic call: g[u] -> Derivative[1][g][u] * u'
                ExprPtr deriv1 = make_normal("Derivative", {make_integer(1)});
                ExprPtr g_prime = make_normal(deriv1, {head});
                return times({make_normal(g_prime, {u}), du});
            }
        }
    }

    return make_integer(0);
}

ExprPtr expand_product(const ExprList& factors) {
    if (factors.empty()) return make_integer(1);
    if (factors.size() == 1) return expand(factors[0]);

    ExprPtr first = expand(factors[0]);
    ExprList rest_factors(factors.begin() + 1, factors.end());
    ExprPtr rest = expand_product(rest_factors);

    if (first->has_head("Plus") && rest->has_head("Plus")) {
        ExprList terms;
        for (const auto& a : first->args()) {
            for (const auto& b : rest->args()) {
                terms.push_back(expand(times({a, b})));
            }
        }
        return plus(std::move(terms));
    }
    if (first->has_head("Plus")) {
        ExprList terms;
        for (const auto& a : first->args()) {
            terms.push_back(expand(times({a, rest})));
        }
        return plus(std::move(terms));
    }
    if (rest->has_head("Plus")) {
        ExprList terms;
        for (const auto& b : rest->args()) {
            terms.push_back(expand(times({first, b})));
        }
        return plus(std::move(terms));
    }

    return times({first, rest});
}

}  // namespace

ExprPtr diff(const ExprPtr& f, const ExprPtr& x) {
    return diff_impl(f, x);
}

ExprPtr expand(const ExprPtr& e) {
    if (!e || e->is_number() || e->is_symbol()) return e;

    if (e->has_head("Plus")) {
        ExprList terms;
        terms.reserve(e->size());
        for (const auto& a : e->args()) {
            terms.push_back(expand(a));
        }
        return plus(std::move(terms));
    }

    if (e->has_head("Times")) {
        return expand_product(e->args());
    }

    if (e->has_head("Power") && e->size() == 2) {
        ExprPtr base = expand(e->arg(0));
        const auto& exp = e->arg(1);

        if (exp->is_integer() && exp->integer().sign() > 0) {
            auto n = exp->integer().to_int64();
            if (n && *n <= 64) {
                ExprList factors;
                factors.reserve(static_cast<std::size_t>(*n));
                for (long long i = 0; i < *n; ++i) {
                    factors.push_back(base);
                }
                return expand_product(factors);
            }
        }
        return power(base, expand(exp));
    }

    if (e->is_normal()) {
        ExprList args;
        args.reserve(e->size());
        for (const auto& a : e->args()) {
            args.push_back(expand(a));
        }
        return make_normal(expand(e->head()), std::move(args));
    }

    return e;
}

}  // namespace symats
