// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"

#include <vector>

namespace symats {
namespace {

const ExprPtr& zero() {
    static const ExprPtr z = make_integer(0);
    return z;
}

const ExprPtr& one() {
    static const ExprPtr o = make_integer(1);
    return o;
}

const ExprPtr& neg_one() {
    static const ExprPtr n = make_integer(-1);
    return n;
}

bool contains_var(const ExprPtr& e, const ExprPtr& var) {
    if (equal(e, var)) return true;
    if (!e->is_normal()) return false;
    for (const auto& a : e->args()) {
        if (contains_var(a, var)) return true;
    }
    return false;
}

}  // namespace

ExprPtr d_derivative(const ExprPtr& expr, const ExprPtr& var) {
    if (!contains_var(expr, var)) return zero();
    if (equal(expr, var)) return one();
    if (expr->is_symbol()) return zero();

    if (expr->has_head("Plus")) {
        ExprList d_terms;
        d_terms.reserve(expr->size());
        for (const auto& term : expr->args()) {
            d_terms.push_back(d_derivative(term, var));
        }
        return plus(std::move(d_terms));
    }

    if (expr->has_head("Times")) {
        // Product rule: d/dx (u1 u2 ... uk) = sum_i (u1 ... ui' ... uk)
        ExprList sum_terms;
        sum_terms.reserve(expr->size());
        for (std::size_t i = 0; i < expr->size(); ++i) {
            ExprPtr du_i = d_derivative(expr->arg(i), var);
            if (du_i->is_integer() && du_i->integer().is_zero()) continue;

            ExprList prod_factors;
            prod_factors.reserve(expr->size());
            for (std::size_t j = 0; j < expr->size(); ++j) {
                if (i == j) prod_factors.push_back(du_i);
                else prod_factors.push_back(expr->arg(j));
            }
            sum_terms.push_back(times(std::move(prod_factors)));
        }
        return plus(std::move(sum_terms));
    }

    if (expr->has_head("Power") && expr->size() == 2) {
        const ExprPtr& u = expr->arg(0);
        const ExprPtr& v = expr->arg(1);

        const bool u_has = contains_var(u, var);
        const bool v_has = contains_var(v, var);

        if (u_has && !v_has) {
            // Power rule: d/dx (u^v) = v * u^(v-1) * u'
            ExprPtr du = d_derivative(u, var);
            ExprPtr exponent = plus({v, neg_one()});
            return times({v, power(u, exponent), du});
        }
        if (!u_has && v_has) {
            // Exponential rule: d/dx (a^v) = a^v * Log(a) * v'
            ExprPtr dv = d_derivative(v, var);
            ExprPtr log_u = make_normal("Log", {u});
            return times({expr, log_u, dv});
        }
        if (u_has && v_has) {
            // General power rule: d/dx (u^v) = u^v * (v' * Log(u) + v * u' / u)
            ExprPtr du = d_derivative(u, var);
            ExprPtr dv = d_derivative(v, var);
            ExprPtr term1 = times({dv, make_normal("Log", {u})});
            ExprPtr term2 = divide(times({v, du}), u);
            return times({expr, plus({term1, term2})});
        }
    }

    if (expr->is_normal() && expr->size() == 1) {
        const std::string& head_name = expr->head()->is_symbol() ? expr->head()->name() : "";
        const ExprPtr& u = expr->arg(0);
        ExprPtr du = d_derivative(u, var);

        if (head_name == "Sin") {
            return times({make_normal("Cos", {u}), du});
        }
        if (head_name == "Cos") {
            return times({neg_one(), make_normal("Sin", {u}), du});
        }
        if (head_name == "Tan") {
            return times({power(make_normal("Sec", {u}), make_integer(2)), du});
        }
        if (head_name == "Cot") {
            return times({neg_one(), power(make_normal("Csc", {u}), make_integer(2)), du});
        }
        if (head_name == "Sec") {
            return times({make_normal("Sec", {u}), make_normal("Tan", {u}), du});
        }
        if (head_name == "Csc") {
            return times({neg_one(), make_normal("Csc", {u}), make_normal("Cot", {u}), du});
        }
        if (head_name == "Exp") {
            return times({make_normal("Exp", {u}), du});
        }
        if (head_name == "Log") {
            return divide(du, u);
        }
        if (head_name == "ArcSin") {
            ExprPtr denom = power(plus({one(), negate(power(u, make_integer(2)))}), make_rational(1, 2));
            return divide(du, denom);
        }
        if (head_name == "ArcCos") {
            ExprPtr denom = power(plus({one(), negate(power(u, make_integer(2)))}), make_rational(1, 2));
            return divide(negate(du), denom);
        }
        if (head_name == "ArcTan") {
            ExprPtr denom = plus({one(), power(u, make_integer(2))});
            return divide(du, denom);
        }
        if (head_name == "Sinh") {
            return times({make_normal("Cosh", {u}), du});
        }
        if (head_name == "Cosh") {
            return times({make_normal("Sinh", {u}), du});
        }
        if (head_name == "Tanh") {
            return times({power(make_normal("Sech", {u}), make_integer(2)), du});
        }

        // Derivative of y'[x] (Derivative(n, f)[x])
        if (expr->head()->has_head("Derivative") && expr->head()->size() == 2 &&
            expr->head()->arg(0)->is_integer()) {
            int64_t n = expr->head()->arg(0)->integer().to_int64().value_or(1);
            ExprPtr new_deriv = make_normal("Derivative", {make_integer(n + 1), expr->head()->arg(1)});
            return times({make_normal(new_deriv, {u}), du});
        }

        // Generic f[u] -> Derivative(1, f)[u] * u'
        if (expr->head()->is_symbol()) {
            ExprPtr deriv_head = make_normal("Derivative", {make_integer(1), expr->head()});
            return times({make_normal(deriv_head, {u}), du});
        }
    }

    if (expr->has_head("Log") && expr->size() == 2) {
        // Log[b, u] -> u' / (u * Log[b])
        const ExprPtr& b = expr->arg(0);
        const ExprPtr& u = expr->arg(1);
        ExprPtr du = d_derivative(u, var);
        return divide(du, times({u, make_normal("Log", {b})}));
    }

    return zero();
}

namespace {

ExprPtr multiply_two(const ExprPtr& a, const ExprPtr& b) {
    if (a->has_head("Plus") && b->has_head("Plus")) {
        ExprList terms;
        terms.reserve(a->size() * b->size());
        for (const auto& ta : a->args()) {
            for (const auto& tb : b->args()) {
                terms.push_back(expand_expr(times(ta, tb)));
            }
        }
        return plus(std::move(terms));
    }
    if (a->has_head("Plus")) {
        ExprList terms;
        terms.reserve(a->size());
        for (const auto& ta : a->args()) {
            terms.push_back(expand_expr(times(ta, b)));
        }
        return plus(std::move(terms));
    }
    if (b->has_head("Plus")) {
        ExprList terms;
        terms.reserve(b->size());
        for (const auto& tb : b->args()) {
            terms.push_back(expand_expr(times(a, tb)));
        }
        return plus(std::move(terms));
    }
    return times(a, b);
}

}  // namespace

ExprPtr expand_expr(const ExprPtr& expr) {
    if (!expr || !expr->is_normal()) return expr;

    // Recursively expand arguments first
    ExprList expanded_args;
    expanded_args.reserve(expr->size());
    for (const auto& arg : expr->args()) {
        expanded_args.push_back(expand_expr(arg));
    }
    ExprPtr e = make_normal(expr->head(), std::move(expanded_args));

    if (e->has_head("Power") && e->size() == 2) {
        const ExprPtr& base = e->arg(0);
        const ExprPtr& exp = e->arg(1);
        if (exp->is_integer() && exp->integer().sign() > 0 && base->has_head("Plus")) {
            auto n_opt = exp->integer().to_int64();
            if (n_opt && *n_opt <= 20) {
                int64_t n = *n_opt;
                ExprPtr result = base;
                for (int64_t i = 1; i < n; ++i) {
                    result = multiply_two(result, base);
                }
                return result;
            }
        }
    }

    if (e->has_head("Times")) {
        ExprPtr result = one();
        for (const auto& factor : e->args()) {
            result = multiply_two(result, factor);
        }
        return result;
    }

    if (e->has_head("Plus")) {
        ExprList terms;
        terms.reserve(e->size());
        for (const auto& t : e->args()) {
            ExprPtr expanded_t = expand_expr(t);
            if (expanded_t->has_head("Plus")) {
                for (const auto& sub_t : expanded_t->args()) terms.push_back(sub_t);
            } else {
                terms.push_back(expanded_t);
            }
        }
        return plus(std::move(terms));
    }

    return e;
}

}  // namespace symats
