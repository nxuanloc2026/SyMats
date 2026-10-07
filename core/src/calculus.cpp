// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Native symbolic differentiation (D) and expansion (Expand).
#include "symats/calculus.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace symats {

namespace {

bool contains_symbol(const ExprPtr& expr, const std::string& var) {
    if (!expr) return false;
    if (expr->is_symbol()) return expr->name() == var;
    if (expr->is_number()) return false;
    for (const auto& arg : expr->args()) {
        if (contains_symbol(arg, var)) return true;
    }
    return contains_symbol(expr->head(), var);
}

ExprPtr distribute2(const ExprPtr& a, const ExprPtr& b) {
    if (a->has_head("Plus") && b->has_head("Plus")) {
        ExprList terms;
        for (const auto& ai : a->args()) {
            for (const auto& bj : b->args()) {
                terms.push_back(times({ai, bj}));
            }
        }
        return plus(std::move(terms));
    }
    if (a->has_head("Plus")) {
        ExprList terms;
        for (const auto& ai : a->args()) {
            terms.push_back(times({ai, b}));
        }
        return plus(std::move(terms));
    }
    if (b->has_head("Plus")) {
        ExprList terms;
        for (const auto& bj : b->args()) {
            terms.push_back(times({a, bj}));
        }
        return plus(std::move(terms));
    }
    return times({a, b});
}

}  // namespace

ExprPtr differentiate(const ExprPtr& expr, const ExprPtr& var) {
    if (!expr || !var || !var->is_symbol())
        throw std::invalid_argument("differentiate: expected valid expression and variable symbol");

    const std::string& v = var->name();

    // Constant w.r.t. var
    if (!contains_symbol(expr, v)) {
        return make_integer(0);
    }

    if (expr->is_symbol()) {
        return expr->name() == v ? make_integer(1) : make_integer(0);
    }

    if (expr->is_number()) {
        return make_integer(0);
    }

    const ExprPtr& head = expr->head();

    // Linearity of sum: D(a + b + c, x) = D(a, x) + D(b, x) + D(c, x)
    if (expr->has_head("Plus")) {
        ExprList terms;
        terms.reserve(expr->size());
        for (const auto& term : expr->args()) {
            terms.push_back(differentiate(term, var));
        }
        return plus(std::move(terms));
    }

    // Product rule: D(u1 * u2 * ... * un, x) = sum_i ( D(ui, x) * prod_{j != i} uj )
    if (expr->has_head("Times")) {
        ExprList terms;
        terms.reserve(expr->size());
        for (std::size_t i = 0; i < expr->size(); ++i) {
            ExprPtr du_i = differentiate(expr->arg(i), var);
            if (du_i->is_integer() && du_i->integer().is_zero()) continue;
            ExprList factors;
            factors.reserve(expr->size());
            factors.push_back(du_i);
            for (std::size_t j = 0; j < expr->size(); ++j) {
                if (j != i) factors.push_back(expr->arg(j));
            }
            terms.push_back(times(std::move(factors)));
        }
        return plus(std::move(terms));
    }

    // Power rule: D(u^v, x)
    if (expr->has_head("Power") && expr->size() == 2) {
        const ExprPtr& u = expr->arg(0);
        const ExprPtr& w = expr->arg(1);
        bool u_dep = contains_symbol(u, v);
        bool w_dep = contains_symbol(w, v);

        if (!u_dep && !w_dep) return make_integer(0);

        if (u_dep && !w_dep) {  // u(x)^c -> c * u(x)^(c - 1) * u'(x)
            ExprPtr du = differentiate(u, var);
            return times({w, power(u, plus({w, make_integer(-1)})), du});
        }

        if (!u_dep && w_dep) {  // c^w(x) -> c^w(x) * Log(c) * w'(x)
            ExprPtr dw = differentiate(w, var);
            return times({expr, make_normal("Log", {u}), dw});
        }

        // u(x)^w(x) -> u(x)^w(x) * ( w'(x)*Log(u(x)) + w(x)*u'(x)/u(x) )
        ExprPtr du = differentiate(u, var);
        ExprPtr dw = differentiate(w, var);
        ExprPtr term1 = times({dw, make_normal("Log", {u})});
        ExprPtr term2 = times({w, du, power(u, make_integer(-1))});
        return times({expr, plus({term1, term2})});
    }

    // Single-argument elementary functions (chain rule)
    if (head->is_symbol() && expr->size() == 1) {
        const std::string& h = head->name();
        const ExprPtr& u = expr->arg(0);
        ExprPtr du = differentiate(u, var);

        if (du->is_integer() && du->integer().is_zero()) return make_integer(0);

        if (h == "Sin") return times({make_normal("Cos", {u}), du});
        if (h == "Cos") return times({make_integer(-1), make_normal("Sin", {u}), du});
        if (h == "Tan") return times({power(make_normal("Cos", {u}), make_integer(-2)), du});
        if (h == "Cot") return times({make_integer(-1), power(make_normal("Sin", {u}), make_integer(-2)), du});
        if (h == "Sec") return times({make_normal("Sec", {u}), make_normal("Tan", {u}), du});
        if (h == "Csc") return times({make_integer(-1), make_normal("Csc", {u}), make_normal("Cot", {u}), du});
        if (h == "ArcSin") return times({power(plus({make_integer(1), times({make_integer(-1), power(u, make_integer(2))})}), make_rational(-1, 2)), du});
        if (h == "ArcCos") return times({make_integer(-1), power(plus({make_integer(1), times({make_integer(-1), power(u, make_integer(2))})}), make_rational(-1, 2)), du});
        if (h == "ArcTan") return times({power(plus({make_integer(1), power(u, make_integer(2))}), make_integer(-1)), du});
        if (h == "Sinh") return times({make_normal("Cosh", {u}), du});
        if (h == "Cosh") return times({make_normal("Sinh", {u}), du});
        if (h == "Tanh") return times({power(make_normal("Cosh", {u}), make_integer(-2)), du});
        if (h == "Exp") return times({expr, du});
        if (h == "Log") return times({power(u, make_integer(-1)), du});
        if (h == "Abs") return times({make_normal(make_normal("Derivative", {make_integer(1), head}), {u}), du});

        // Unknown function f(u) -> Derivative(1, f)(u) * u'
        ExprPtr deriv = make_normal(make_normal("Derivative", {make_integer(1), head}), {u});
        if (du->is_integer() && du->integer().is_one()) return deriv;
        return times({deriv, du});
    }

    // Multi-argument unknown function f(u, v, ...) -> sum_i Derivative(..., 1, ... f)(u, v) * u_i'
    if (head->is_symbol() && expr->size() > 1) {
        ExprList terms;
        for (std::size_t i = 0; i < expr->size(); ++i) {
            ExprPtr du_i = differentiate(expr->arg(i), var);
            if (du_i->is_integer() && du_i->integer().is_zero()) continue;
            ExprList orders;
            for (std::size_t j = 0; j < expr->size(); ++j) {
                orders.push_back(make_integer(j == i ? 1 : 0));
            }
            orders.push_back(head);
            ExprPtr deriv_head = make_normal("Derivative", std::move(orders));
            terms.push_back(times({make_normal(deriv_head, expr->args()), du_i}));
        }
        return plus(std::move(terms));
    }

    return make_integer(0);
}

ExprPtr differentiate(const ExprPtr& expr, const ExprPtr& var, unsigned long long n) {
    ExprPtr result = expr;
    for (unsigned long long i = 0; i < n; ++i) {
        result = differentiate(result, var);
    }
    return result;
}

ExprPtr expand(const ExprPtr& expr) {
    if (!expr || expr->is_number() || expr->is_symbol()) return expr;

    ExprList expanded_args;
    expanded_args.reserve(expr->size());
    for (const auto& arg : expr->args()) {
        expanded_args.push_back(expand(arg));
    }

    ExprPtr head = expand(expr->head());

    if (expr->has_head("Plus")) {
        return plus(std::move(expanded_args));
    }

    if (expr->has_head("Times")) {
        if (expanded_args.empty()) return make_integer(1);
        ExprPtr acc = expanded_args[0];
        for (std::size_t i = 1; i < expanded_args.size(); ++i) {
            acc = distribute2(acc, expanded_args[i]);
        }
        return acc;
    }

    if (expr->has_head("Power") && expanded_args.size() == 2) {
        const ExprPtr& base = expanded_args[0];
        const ExprPtr& exponent = expanded_args[1];

        if (base->has_head("Plus") && exponent->is_integer() && !exponent->integer().is_negative()) {
            auto n = exponent->integer().to_int64();
            if (n && *n > 0 && *n <= 100) {
                ExprPtr acc = base;
                for (long long i = 1; i < *n; ++i) {
                    acc = distribute2(acc, base);
                }
                return acc;
            }
        }
        return power(base, exponent);
    }

    return make_normal(std::move(head), std::move(expanded_args));
}

}  // namespace symats
