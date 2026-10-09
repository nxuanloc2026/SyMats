// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/native_backend.h"

#include <cmath>

#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/numeric.h"
#include "symats/pattern.h"
#include "symats/verification.h"

namespace symats {
namespace {

bool free_of(const ExprPtr& e, const ExprPtr& x) {
    if (equal(e, x)) return false;
    if (!e->is_normal()) return true;
    if (!free_of(e->head(), x)) return false;
    for (const auto& a : e->args())
        if (!free_of(a, x)) return false;
    return true;
}

ExprPtr simplify(const ExprPtr& e) {
    Context ctx;
    return evaluate(e, ctx);
}

// u = a*x + b with a != 0 free of x: returns a, else nullptr.
ExprPtr linear_coefficient(const ExprPtr& u, const ExprPtr& x) {
    if (free_of(u, x)) return nullptr;
    const ExprPtr a = simplify(differentiate(u, x));
    if (!free_of(a, x) || (a->is_integer() && a->integer().is_zero())) return nullptr;
    return a;
}

ExprPtr fn(const char* head, const ExprPtr& u) { return make_normal(head, {u}); }

// Antiderivative of a single factor (no constant factors left).
ExprPtr basic(const ExprPtr& f, const ExprPtr& x) {
    if (equal(f, x)) return divide(power(x, make_integer(2)), make_integer(2));
    if (f->has_head("Power") && f->size() == 2) {
        const ExprPtr &base = f->arg(0), &n = f->arg(1);
        if (free_of(n, x)) {  // (a x + b)^n
            if (const ExprPtr a = linear_coefficient(base, x)) {
                if (n->is_integer() && n->integer() == Integer(-1)) return divide(fn("Log", base), a);
                const ExprPtr n1 = plus(n, make_integer(1));
                return divide(power(base, n1), times(a, n1));
            }
        }
        if (free_of(base, x)) {  // c^(a x + b)
            const ExprPtr a = linear_coefficient(n, x);
            if (!a) return nullptr;
            if (base->is_symbol("E")) return divide(f, a);
            return divide(f, times(a, fn("Log", base)));
        }
        // 1/(1 + x^2), 1/Sqrt[1 - x^2]
        const ExprPtr minus_one = make_integer(-1), minus_half = make_rational(-1, 2);
        const ExprPtr one_plus = plus(make_integer(1), power(x, make_integer(2)));
        const ExprPtr one_minus = subtract(make_integer(1), power(x, make_integer(2)));
        if (equal(n, minus_one) && equal(simplify(base), simplify(one_plus))) return fn("ArcTan", x);
        if (equal(n, minus_half) && equal(simplify(base), simplify(one_minus))) return fn("ArcSin", x);
        return nullptr;
    }
    if (f->is_normal() && f->head()->is_symbol() && f->size() == 1) {
        const ExprPtr& u = f->arg(0);
        const ExprPtr a = linear_coefficient(u, x);
        if (!a) return nullptr;
        const std::string& h = f->head()->name();
        ExprPtr F;
        if (h == "Exp") F = f;
        else if (h == "Sin") F = negate(fn("Cos", u));
        else if (h == "Cos") F = fn("Sin", u);
        else if (h == "Tan") F = negate(fn("Log", fn("Cos", u)));
        else if (h == "Cot") F = fn("Log", fn("Sin", u));
        else if (h == "Sinh") F = fn("Cosh", u);
        else if (h == "Cosh") F = fn("Sinh", u);
        else if (h == "Tanh") F = fn("Log", fn("Cosh", u));
        else if (h == "Log") F = subtract(times(u, fn("Log", u)), u);
        else return nullptr;
        return divide(F, a);
    }
    return nullptr;
}

ExprPtr squared_trig(const ExprPtr& f, const ExprPtr& x) {
    if (!f->has_head("Power") || f->size() != 2 || !equal(f->arg(1), make_integer(2))) return nullptr;
    const ExprPtr& g = f->arg(0);
    if (!g->is_normal() || g->size() != 1) return nullptr;
    const ExprPtr a = linear_coefficient(g->arg(0), x);
    if (!a) return nullptr;
    if (g->has_head("Sec")) return divide(fn("Tan", g->arg(0)), a);
    if (g->has_head("Csc")) return negate(divide(fn("Cot", g->arg(0)), a));
    return nullptr;
}

ExprPtr antiderivative(const ExprPtr& f, const ExprPtr& x, int depth) {
    if (depth > 8) return nullptr;
    if (free_of(f, x)) return times(f, x);
    if (f->has_head("Plus")) {
        ExprList terms;
        for (const auto& t : f->args()) {
            ExprPtr F = antiderivative(t, x, depth + 1);
            if (!F) return nullptr;
            terms.push_back(F);
        }
        return plus(std::move(terms));
    }
    if (f->has_head("Times")) {
        ExprList constant, rest;
        for (const auto& factor : f->args()) (free_of(factor, x) ? constant : rest).push_back(factor);
        if (!constant.empty()) {
            ExprPtr F = antiderivative(times(std::move(rest)), x, depth + 1);
            return F ? times(times(std::move(constant)), F) : nullptr;
        }
    }
    if (ExprPtr F = basic(f, x)) return F;
    if (ExprPtr F = squared_trig(f, x)) return F;
    // Products and powers of sums: expand and try term by term.
    const ExprPtr expanded = expand(f);
    if (!equal(expanded, f)) return antiderivative(expanded, x, depth + 1);
    return nullptr;
}

// Tan, Cot, Sec, Csc in terms of Sin and Cos, so that exact comparison sees through them.
ExprPtr sin_cos(const ExprPtr& e) {
    if (!e->is_normal()) return e;
    ExprList args;
    for (const auto& a : e->args()) args.push_back(sin_cos(a));
    if (args.size() == 1 && e->head()->is_symbol()) {
        const auto& h = e->head()->name();
        const ExprPtr s = fn("Sin", args[0]), c = fn("Cos", args[0]);
        if (h == "Tan") return divide(s, c);
        if (h == "Cot") return divide(c, s);
        if (h == "Sec") return power(c, make_integer(-1));
        if (h == "Csc") return power(s, make_integer(-1));
    }
    return make_normal(sin_cos(e->head()), std::move(args));
}

std::optional<double> number(const ExprPtr& e) {
    try {
        const double v = numeric::constant(e);
        if (std::isfinite(v)) return v;
    } catch (const numeric::Unsupported&) {}
    return std::nullopt;
}

}  // namespace

ExprPtr native_antiderivative(const ExprPtr& f, const ExprPtr& x) {
    if (!x->is_symbol()) return nullptr;
    ExprPtr F = antiderivative(f, x, 0);
    if (!F) return nullptr;
    F = simplify(F);
    // Only results that differentiate back to f exactly are trusted.
    if (verify_backend_result(make_normal("Integrate", {f, x}), F)) return F;
    const ExprPtr difference = simplify(expand(sin_cos(subtract(differentiate(F, x), f))));
    return difference->is_integer() && difference->integer().is_zero() ? F : nullptr;
}

namespace {
bool singular(const ExprPtr& e) {
    if (e->is_symbol())
        return e->is_symbol("ComplexInfinity") || e->is_symbol("Indeterminate") || e->is_symbol("Infinity");
    if (!e->is_normal()) return false;
    if (e->has_head("Log") && e->size() == 1 && e->arg(0)->is_integer() && e->arg(0)->integer().is_zero())
        return true;
    if (singular(e->head())) return true;
    for (const auto& a : e->args())
        if (singular(a)) return true;
    return false;
}

std::optional<BackendResult> taylor(const ExprPtr& expr) {
    const ExprPtr& f = expr->arg(0);
    const ExprPtr& spec = expr->arg(1);
    if (!spec->has_head("List") || spec->size() != 3 || !spec->arg(0)->is_symbol() || !spec->arg(2)->is_integer())
        return std::nullopt;
    const ExprPtr& x = spec->arg(0);
    const ExprPtr& a = spec->arg(1);
    const auto n = spec->arg(2)->integer().to_int64();
    if (!n || *n < 0 || *n > 12 || !free_of(a, x)) return std::nullopt;
    ExprList terms;
    ExprPtr d = f;
    Integer factorial = 1;
    for (long long k = 0; k <= *n; ++k) {
        if (k > 0) {
            d = simplify(differentiate(d, x));
            factorial *= Integer(k);
        }
        const ExprPtr c = simplify(substitute(d, {{x->name(), a}}));
        if (singular(c)) return std::nullopt;
        terms.push_back(times({c, make_rational(1, factorial), power(subtract(x, a), make_integer(k))}));
    }
    return BackendResult{make_normal("SeriesData", {simplify(plus(std::move(terms))), spec}),
                         ResultStatus::Exact, "native"};
}
// ---------------------------------------------------------------- Solve

bool is_zero_expr(const ExprPtr& e) { return e->is_integer() && e->integer().is_zero(); }

// Coefficients c0..cd of p as a polynomial in x (degree <= max_degree), or nullopt.
std::optional<ExprList> coefficients(const ExprPtr& p, const ExprPtr& x, int max_degree) {
    ExprList c;
    ExprPtr d = simplify(expand(p));
    Integer factorial = 1;
    for (int k = 0; k <= max_degree + 1; ++k) {
        if (k > 0) {
            d = simplify(expand(differentiate(d, x)));
            factorial *= Integer(k);
        }
        if (is_zero_expr(d)) return c;
        if (k == max_degree + 1) return std::nullopt;
        const ExprPtr ck = simplify(substitute(d, {{x->name(), make_integer(0)}}));
        if (!free_of(ck, x) || singular(ck)) return std::nullopt;
        c.push_back(simplify(times(ck, make_rational(1, factorial))));
    }
    return std::nullopt;
}

ExprPtr rule(const ExprPtr& x, const ExprPtr& v) { return make_normal("Rule", {x, simplify(v)}); }
ExprPtr branch(ExprList rules) { return make_normal("List", std::move(rules)); }

std::optional<ExprPtr> solve_one(const ExprPtr& p, const ExprPtr& x) {
    auto c = coefficients(p, x, 2);
    if (!c) return std::nullopt;
    while (!c->empty() && is_zero_expr(c->back())) c->pop_back();
    if (c->size() == 2)  // c0 + c1 x
        return make_normal("List", {branch({rule(x, negate(divide((*c)[0], (*c)[1])))})});
    if (c->size() == 3) {  // c0 + c1 x + c2 x^2
        const ExprPtr &c0 = (*c)[0], &c1 = (*c)[1], &c2 = (*c)[2];
        const ExprPtr disc = simplify(subtract(power(c1, make_integer(2)), times({make_integer(4), c2, c0})));
        const ExprPtr root = power(disc, make_rational(1, 2));
        const ExprPtr two_a = times(make_integer(2), c2);
        if (is_zero_expr(disc)) return make_normal("List", {branch({rule(x, divide(negate(c1), two_a))})});
        return make_normal("List", {branch({rule(x, divide(subtract(negate(c1), root), two_a))}),
                                    branch({rule(x, divide(plus(negate(c1), root), two_a))})});
    }
    return std::nullopt;
}

// Determinant by cofactor expansion (small symbolic matrices).
ExprPtr det(const std::vector<ExprList>& m) {
    const std::size_t n = m.size();
    if (n == 1) return m[0][0];
    ExprList terms;
    for (std::size_t j = 0; j < n; ++j) {
        std::vector<ExprList> minor;
        for (std::size_t i = 1; i < n; ++i) {
            ExprList row;
            for (std::size_t k = 0; k < n; ++k)
                if (k != j) row.push_back(m[i][k]);
            minor.push_back(std::move(row));
        }
        ExprPtr t = times(m[0][j], det(minor));
        terms.push_back(j % 2 ? negate(t) : t);
    }
    return simplify(plus(std::move(terms)));
}

std::optional<ExprPtr> solve_linear_system(const ExprList& polys, const ExprList& vars) {
    const std::size_t n = vars.size();
    if (polys.size() != n || n > 6) return std::nullopt;
    std::vector<ExprList> a(n, ExprList(n));
    ExprList b(n);
    bool numeric_matrix = true;
    for (std::size_t i = 0; i < n; ++i) {
        ExprPtr rest = polys[i];
        for (std::size_t j = 0; j < n; ++j) {
            a[i][j] = simplify(differentiate(polys[i], vars[j]));
            for (const auto& v : vars)
                if (!free_of(a[i][j], v)) return std::nullopt;  // not linear
            numeric_matrix = numeric_matrix && a[i][j]->is_number();
        }
        Bindings zero;
        for (const auto& v : vars) zero[v->name()] = make_integer(0);
        b[i] = simplify(negate(substitute(polys[i], zero)));
        numeric_matrix = numeric_matrix && b[i]->is_number();
    }
    ExprList values;
    if (numeric_matrix) {
        ExprList rows;
        for (const auto& r : a) rows.push_back(make_normal("List", r));
        const ExprPtr x = simplify(make_normal("LinearSolve", {make_normal("List", rows), make_normal("List", b)}));
        if (!x->has_head("List") || x->size() != n) return std::nullopt;
        values = x->args();
    } else {
        if (n > 3) return std::nullopt;
        const ExprPtr d = det(a);
        if (is_zero_expr(d)) return std::nullopt;
        for (std::size_t j = 0; j < n; ++j) {  // Cramer's rule
            std::vector<ExprList> aj = a;
            for (std::size_t i = 0; i < n; ++i) aj[i][j] = b[i];
            values.push_back(divide(det(aj), d));
        }
    }
    ExprList rules;
    for (std::size_t j = 0; j < n; ++j) rules.push_back(rule(vars[j], values[j]));
    return make_normal("List", {branch(std::move(rules))});
}

std::optional<BackendResult> solve(const ExprPtr& expr) {
    const ExprList eqs = expr->arg(0)->has_head("List") ? expr->arg(0)->args() : ExprList{expr->arg(0)};
    const ExprList vars = expr->arg(1)->has_head("List") ? expr->arg(1)->args() : ExprList{expr->arg(1)};
    if (eqs.empty() || vars.empty()) return std::nullopt;
    ExprList polys;
    for (const auto& eq : eqs) {
        if (!eq->has_head("Equal") || eq->size() != 2) return std::nullopt;
        polys.push_back(subtract(eq->arg(0), eq->arg(1)));
    }
    for (const auto& v : vars)
        if (!v->is_symbol()) return std::nullopt;
    std::optional<ExprPtr> result;
    if (polys.size() == 1 && vars.size() == 1) result = solve_one(polys[0], vars[0]);
    else result = solve_linear_system(polys, vars);
    if (!result || verification_status(expr, *result) == ResultStatus::Unverified) return std::nullopt;
    return BackendResult{*result, ResultStatus::Exact, "native"};
}
}  // namespace

std::optional<BackendResult> NativeBackend::evaluate(const ExprPtr& expr) {
    if (expr && expr->has_head("Series") && expr->size() == 2) return taylor(expr);
    if (expr && expr->has_head("Solve") && expr->size() == 2) return solve(expr);
    if (!expr || !expr->has_head("Integrate") || expr->size() != 2) return std::nullopt;
    const ExprPtr& f = expr->arg(0);
    const ExprPtr& range = expr->arg(1);
    if (range->is_symbol()) {
        if (ExprPtr F = native_antiderivative(f, range)) return BackendResult{F, ResultStatus::Exact, "native"};
        return std::nullopt;
    }
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol()) return std::nullopt;
    const ExprPtr& x = range->arg(0);
    if (ExprPtr F = native_antiderivative(f, x)) {
        const ExprPtr value = simplify(subtract(substitute(F, {{x->name(), range->arg(2)}}),
                                                substitute(F, {{x->name(), range->arg(1)}})));
        // F[b] - F[a] is wrong across a singularity (e.g. 1/x on [-1, 1]); a quadrature
        // check of the closed form catches that.
        if (verification_status(expr, value) != ResultStatus::Unverified)
            return BackendResult{value, ResultStatus::Exact, "native"};
    }
    const auto a = number(range->arg(1)), b = number(range->arg(2));
    if (!a || !b) return std::nullopt;
    try {
        numeric::compile(f, {{x->name(), 0}});  // numeric integrand: no free parameters
    } catch (const numeric::Unsupported&) {
        return std::nullopt;
    }
    return BackendResult{make_normal("NIntegrate", {f, range}), ResultStatus::Numeric, "native"};
}

}  // namespace symats
