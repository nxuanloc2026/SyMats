// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"

#include <stdexcept>
#include <utility>

// Product and chain rules follow standard single-variable calculus; the expansion
// uses distributivity and the multinomial theorem (see Apostol, Calculus, vol. 1).

namespace symats {
namespace {

ExprPtr integer(long long value) { return make_integer(value); }
ExprPtr fn(std::string_view name, ExprPtr arg) { return make_normal(name, {std::move(arg)}); }
ExprPtr derivative_application(ExprPtr order, ExprPtr function, ExprList args) {
    ExprPtr derivative_head = make_normal("Derivative", {std::move(order)});
    return make_normal(make_normal(derivative_head, {std::move(function)}), std::move(args));
}
bool is_zero(const ExprPtr& e) { return e->is_integer() && e->integer().is_zero(); }

bool free_of(const ExprPtr& e, const ExprPtr& variable) {
    if (equal(e, variable)) return false;
    if (!e->is_normal()) return true;
    if (!free_of(e->head(), variable)) return false;
    for (const auto& a : e->args()) if (!free_of(a, variable)) return false;
    return true;
}

ExprPtr derivative(const ExprPtr& e, const ExprPtr& variable, std::size_t depth) {
    if (depth > 200) throw std::invalid_argument("D: expression nesting limit exceeded");
    if (equal(e, variable)) return integer(1);
    if (free_of(e, variable)) return integer(0);
    if (!e->is_normal()) return integer(0);

    if (e->has_head("Plus")) {
        ExprList terms;
        for (const auto& a : e->args()) terms.push_back(derivative(a, variable, depth + 1));
        return plus(std::move(terms));
    }
    if (e->has_head("Times")) {
        ExprList terms;
        for (std::size_t i = 0; i < e->size(); ++i) {
            ExprPtr d = derivative(e->arg(i), variable, depth + 1);
            if (is_zero(d)) continue;
            ExprList factors = e->args();
            factors[i] = d;
            terms.push_back(times(std::move(factors)));
        }
        return plus(std::move(terms));
    }
    if (e->has_head("Power") && e->size() == 2) {
        const ExprPtr& base = e->arg(0);
        const ExprPtr& exponent = e->arg(1);
        ExprPtr db = derivative(base, variable, depth + 1);
        if (free_of(exponent, variable))
            return times({exponent, power(base, subtract(exponent, integer(1))), db});
        ExprPtr de = derivative(exponent, variable, depth + 1);
        ExprPtr log_base = base->is_symbol("E") ? integer(1) : fn("Log", base);
        return times(e, plus(times(de, log_base),
                             divide(times(exponent, db), base)));
    }

    if (e->head()->is_normal() && e->head()->size() == 1 &&
        e->head()->head()->has_head("Derivative") && e->head()->head()->size() == 1) {
        const ExprPtr& orders = e->head()->head()->arg(0);
        const ExprPtr& function = e->head()->arg(0);
        ExprList terms;
        if (orders->is_integer() && e->size() == 1) {
            ExprPtr du = derivative(e->arg(0), variable, depth + 1);
            if (is_zero(du)) return du;
            ExprPtr next = make_integer(orders->integer() + Integer(1));
            return times(derivative_application(next, function, e->args()), du);
        }
        if (orders->has_head("List") && orders->size() == e->size()) {
            for (std::size_t i = 0; i < e->size(); ++i) {
                if (!orders->arg(i)->is_integer()) return make_normal("D", {e, variable});
                ExprPtr d = derivative(e->arg(i), variable, depth + 1);
                if (is_zero(d)) continue;
                ExprList next_orders = orders->args();
                next_orders[i] = make_integer(next_orders[i]->integer() + Integer(1));
                terms.push_back(times(derivative_application(
                    make_normal("List", std::move(next_orders)), function, e->args()), d));
            }
            return plus(std::move(terms));
        }
    }

    if (e->size() == 1 && e->head()->is_symbol()) {
        const ExprPtr& u = e->arg(0);
        ExprPtr du = derivative(u, variable, depth + 1);
        if (is_zero(du)) return du;
        const std::string& h = e->head()->name();
        ExprPtr outer;
        if (h == "Sin") outer = fn("Cos", u);
        else if (h == "Cos") outer = negate(fn("Sin", u));
        else if (h == "Tan") outer = power(fn("Cos", u), integer(-2));
        else if (h == "Cot") outer = negate(power(fn("Csc", u), integer(2)));
        else if (h == "Sec") outer = times(fn("Sec", u), fn("Tan", u));
        else if (h == "Csc") outer = negate(times(fn("Csc", u), fn("Cot", u)));
        else if (h == "Exp") outer = e;
        else if (h == "Log") outer = power(u, integer(-1));
        else if (h == "ArcSin") outer = power(subtract(integer(1), power(u, integer(2))),
                                                 make_rational(-1, 2));
        else if (h == "ArcCos") outer = negate(power(subtract(integer(1), power(u, integer(2))),
                                                      make_rational(-1, 2)));
        else if (h == "ArcTan") outer = power(plus(integer(1), power(u, integer(2))), integer(-1));
        else if (h == "ArcSinh") outer = power(plus(integer(1), power(u, integer(2))), make_rational(-1, 2));
        else if (h == "ArcCosh") outer = power(subtract(power(u, integer(2)), integer(1)), make_rational(-1, 2));
        else if (h == "ArcTanh") outer = power(subtract(integer(1), power(u, integer(2))), integer(-1));
        else if (h == "Sinh") outer = fn("Cosh", u);
        else if (h == "Cosh") outer = fn("Sinh", u);
        else if (h == "Tanh") outer = power(fn("Cosh", u), integer(-2));
        else if (h == "Abs") outer = divide(u, e);
        else {
            outer = derivative_application(integer(1), e->head(), {u});
        }
        return times(outer, du);
    }
    if (e->has_head("Log") && e->size() == 2) {
        const ExprPtr& base = e->arg(0);
        const ExprPtr& argument = e->arg(1);
        ExprPtr db = derivative(base, variable, depth + 1);
        ExprPtr da = derivative(argument, variable, depth + 1);
        ExprPtr log_base = fn("Log", base);
        if (is_zero(db)) return divide(da, times(argument, log_base));
        ExprPtr denominator = times({base, power(log_base, integer(2))});
        if (is_zero(da)) return negate(divide(times(fn("Log", argument), db), denominator));
        return subtract(divide(da, times(argument, log_base)),
                        divide(times(fn("Log", argument), db), denominator));
    }
    if (e->head()->is_symbol() && e->size() > 1) {
        ExprList terms;
        for (std::size_t i = 0; i < e->size(); ++i) {
            ExprPtr d = derivative(e->arg(i), variable, depth + 1);
            if (is_zero(d)) continue;
            ExprList orders(e->size(), integer(0));
            orders[i] = integer(1);
            ExprPtr partial = derivative_application(
                make_normal("List", std::move(orders)), e->head(), e->args());
            terms.push_back(times(partial, d));
        }
        return plus(std::move(terms));
    }
    return make_normal("D", {e, variable});
}

ExprPtr expand_impl(const ExprPtr& e, std::size_t depth) {
    if (depth > 200) throw std::invalid_argument("Expand: expression nesting limit exceeded");
    if (!e->is_normal()) return e;
    if (e->has_head("Plus")) {
        ExprList terms;
        for (const auto& a : e->args()) terms.push_back(expand_impl(a, depth + 1));
        return plus(std::move(terms));
    }
    if (e->has_head("Times")) {
        ExprList terms{integer(1)};
        for (const auto& factor : e->args()) {
            ExprPtr f = expand_impl(factor, depth + 1);
            ExprList next;
            if (f->has_head("Plus")) {
                if (f->size() != 0 && terms.size() > 20000 / f->size())
                    throw std::invalid_argument("Expand: term limit exceeded");
                for (const auto& term : terms)
                    for (const auto& addend : f->args()) next.push_back(times(term, addend));
            } else {
                for (const auto& term : terms) next.push_back(times(term, f));
            }
            terms = std::move(next);
        }
        return plus(std::move(terms));
    }
    if (e->has_head("Power") && e->size() == 2) {
        ExprPtr base = expand_impl(e->arg(0), depth + 1);
        const ExprPtr& exponent = e->arg(1);
        if (base->has_head("Plus") && exponent->is_integer()) {
            const auto n = exponent->integer().to_int64();
            if (n && *n >= 0) {
                if (*n > 20000) throw std::invalid_argument("Expand: term limit exceeded");
                ExprPtr result = integer(1);
                for (long long i = 0; i < *n; ++i)
                    result = expand_impl(make_normal("Times", {result, base}), depth + 1);
                return result;
            }
        }
        return power(base, exponent);
    }
    ExprList args;
    for (const auto& a : e->args()) args.push_back(expand_impl(a, depth + 1));
    return make_normal(e->head(), std::move(args));
}

}  // namespace

ExprPtr differentiate(const ExprPtr& expression, const ExprPtr& variable) {
    if (!expression || !variable || !variable->is_symbol())
        throw std::invalid_argument("D expects an expression and a symbol variable");
    return derivative(expression, variable, 0);
}

ExprPtr expand(const ExprPtr& expression) {
    if (!expression) throw std::invalid_argument("Expand expects an expression");
    return expand_impl(expression, 0);
}

}  // namespace symats
