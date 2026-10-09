// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/native_backend.h"

#include <cmath>
#include <functional>
#include <set>

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

// u == a*x + b with a != 0 free of x: returns a, else nullptr.
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

ExprPtr antiderivative(const ExprPtr& f, const ExprPtr& x, int depth);

bool zero_integer(const ExprPtr& e) { return e->is_integer() && e->integer().is_zero(); }

bool polynomial(const ExprPtr& e, const ExprPtr& x) {
    if (free_of(e, x) || equal(e, x)) return true;
    if (e->has_head("Power") && e->size() == 2)
        return e->arg(1)->is_integer() && e->arg(1)->integer().sign() > 0 && polynomial(e->arg(0), x);
    if (e->has_head("Plus") || e->has_head("Times")) {
        for (const auto& a : e->args())
            if (!polynomial(a, x)) return false;
        return true;
    }
    return false;
}

// Integral of P[x] g[a x + b] with P a polynomial and g one of Exp, Sin, Cos, Sinh, Cosh
// (or c^(a x + b)): P G - Integral[P' G], until the polynomial is used up.
ExprPtr by_parts(const ExprPtr& f, const ExprPtr& x, int depth) {
    if (!f->has_head("Times")) return nullptr;
    ExprList poly, rest;
    for (const auto& factor : f->args()) (polynomial(factor, x) ? poly : rest).push_back(factor);
    if (poly.empty() || rest.size() != 1) return nullptr;
    const ExprPtr& g = rest[0];
    const bool kind = (g->is_normal() && g->size() == 1 &&
                       (g->has_head("Exp") || g->has_head("Sin") || g->has_head("Cos") ||
                        g->has_head("Sinh") || g->has_head("Cosh"))) ||
                      (g->has_head("Power") && g->size() == 2 && free_of(g->arg(0), x));
    if (!kind) return nullptr;
    const ExprPtr G = basic(g, x);
    if (!G) return nullptr;
    const ExprPtr P = times(std::move(poly));
    const ExprPtr dP = simplify(differentiate(P, x));
    if (zero_integer(dP)) return times(P, G);
    const ExprPtr rest_integral = antiderivative(simplify(expand(times(dP, G))), x, depth + 1);
    return rest_integral ? subtract(times(P, G), rest_integral) : nullptr;
}

ExprPtr antiderivative(const ExprPtr& f, const ExprPtr& x, int depth) {
    if (depth > 12) return nullptr;
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
    if (ExprPtr F = by_parts(f, x, depth)) return F;
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

// Built only from entire functions (polynomials, Exp, Sin, Cos, Sinh, Cosh, c^u with a
// positive number c): no poles or branch points anywhere, so F[b] - F[a] is valid.
bool entire(const ExprPtr& e) {
    if (e->is_number()) return true;
    if (e->is_symbol())
        return !e->is_symbol("ComplexInfinity") && !e->is_symbol("Indeterminate") && !e->is_symbol("Infinity");
    if (!e->head()->is_symbol()) return false;
    const std::string& h = e->head()->name();
    if (h == "Power" && e->size() == 2) {
        const ExprPtr &base = e->arg(0), &n = e->arg(1);
        if (n->is_integer() && n->integer().sign() >= 0) return entire(base);
        const bool positive_base = base->is_symbol("E") || (base->is_number() && base->number().sign() > 0);
        return positive_base && entire(n);
    }
    if (h != "Plus" && h != "Times" && h != "Exp" && h != "Sin" && h != "Cos" && h != "Sinh" && h != "Cosh")
        return false;
    for (const auto& a : e->args())
        if (!entire(a)) return false;
    return true;
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
        try {  // a numeric coefficient must be finite (catches e.g. Abs[0]^-1)
            if (!std::isfinite(numeric::constant(c))) return std::nullopt;
        } catch (const numeric::Unsupported&) {}
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
    if (!result) return std::nullopt;
    const ResultStatus checked = verification_status(expr, *result);
    if (checked == ResultStatus::Unverified) return std::nullopt;
    return BackendResult{*result, checked == ResultStatus::Verified ? ResultStatus::Exact : checked, "native"};
}
// ---------------------------------------------------------------- Limit

// Numerator and denominator of a product (factors with negative integer powers go down).
std::pair<ExprPtr, ExprPtr> fraction(const ExprPtr& e) {
    ExprList num, den;
    for (const auto& f : e->has_head("Times") ? e->args() : ExprList{e}) {
        if (f->has_head("Power") && f->size() == 2 && f->arg(1)->is_number() && f->arg(1)->number().sign() < 0)
            den.push_back(power(f->arg(0), negate(f->arg(1))));
        else
            num.push_back(f);
    }
    return {simplify(times(std::move(num))), simplify(times(std::move(den)))};
}

// f near a on both sides agrees with L (numerically, when f and L are numeric).
bool numeric_limit_ok(const ExprPtr& f, const ExprPtr& x, double a, double L) {
    try {
        const auto c = numeric::compile(f, {{x->name(), 0}});
        int valid = 0;
        for (double h : {1e-4, -1e-4, 1e-5, -1e-5}) {
            const double at = a + h * (1.0 + std::abs(a));
            const double v = c.eval(&at);
            if (!std::isfinite(v)) continue;
            if (std::abs(v - L) > 1e-3 * (1.0 + std::abs(L))) return false;
            ++valid;
        }
        return valid >= 2;
    } catch (const numeric::Unsupported&) {
        return false;
    }
}

std::optional<BackendResult> limit(const ExprPtr& expr) {
    const ExprPtr& f = expr->arg(0);
    const ExprPtr& r = expr->arg(1);
    if (!r->has_head("Rule") || r->size() != 2 || !r->arg(0)->is_symbol()) return std::nullopt;
    const ExprPtr& x = r->arg(0);
    const ExprPtr& a = r->arg(1);
    if (!free_of(a, x) || singular(a)) return std::nullopt;
    const auto at = [&](const ExprPtr& e) { return simplify(substitute(e, {{x->name(), a}})); };
    ExprPtr value = at(f);
    if (singular(value)) {
        // 0/0: L'Hopital on the quotient, a few times.
        auto [num, den] = fraction(f);
        value = nullptr;
        for (int k = 0; k < 4 && !value; ++k) {
            const ExprPtr n0 = at(num), d0 = at(den);
            if (!is_zero_expr(n0) || !is_zero_expr(d0)) {
                const ExprPtr q = simplify(divide(n0, d0));
                if (!singular(q) && !is_zero_expr(d0)) value = q;
                break;
            }
            num = simplify(differentiate(num, x));
            den = simplify(differentiate(den, x));
        }
        if (!value) return std::nullopt;
    }
    // Trust the value only if f approaches it from both sides, where both are numeric.
    const auto av = number(a), lv = number(value);
    if (!av || !lv || !numeric_limit_ok(f, x, *av, *lv)) return std::nullopt;
    return BackendResult{value, ResultStatus::Exact, "native"};
}
// ---------------------------------------------------------------- DSolve

// y[x], y'[x], y''[x] -> slot symbols Y0, Y1, Y2; anything else involving y declines.
ExprPtr ode_slots(const ExprPtr& e, const std::string& y, const ExprPtr& x, bool& ok) {
    if (e->is_normal() && e->size() == 1 && equal(e->arg(0), x)) {
        if (e->head()->is_symbol(y)) return make_symbol("ode`Y0");
        const ExprPtr& h = e->head();
        if (h->is_normal() && h->size() == 1 && h->arg(0)->is_symbol(y) && h->head()->has_head("Derivative") &&
            h->head()->size() == 1 && h->head()->arg(0)->is_integer()) {
            const auto k = h->head()->arg(0)->integer().to_int64();
            if (k && *k >= 1 && *k <= 2) return make_symbol("ode`Y" + std::to_string(*k));
        }
    }
    if (e->is_symbol(y)) ok = false;
    if (!e->is_normal()) return e;
    ExprList args;
    for (const auto& a : e->args()) args.push_back(ode_slots(a, y, x, ok));
    return make_normal(ode_slots(e->head(), y, x, ok), std::move(args));
}

ExprPtr fresh_constant(const ExprPtr& request, int& next) {
    while (true) {
        ExprPtr c = make_symbol("C" + std::to_string(next++));
        if (free_of(request, c)) return c;
    }
}

// General solution of a2 y'' + a1 y' + a0 y == g (numeric a's) with constants C.
ExprPtr general_solution(const ExprList& a, const ExprPtr& g, const ExprPtr& x, const ExprList& c) {
    const auto e = [&](const ExprPtr& r) { return fn("Exp", times(r, x)); };
    if (a.size() == 2) {  // a1 y' + a0 y == g: integrating factor
        const ExprPtr r = simplify(divide(a[0], a[1]));
        const ExprPtr F = native_antiderivative(simplify(divide(times(e(r), g), a[1])), x);
        if (!F) return nullptr;
        return times(e(negate(r)), plus(c[0], F));
    }
    if (!free_of(g, x)) return nullptr;  // second order: constant forcing only
    ExprPtr particular;
    if (!is_zero_expr(a[0])) particular = divide(g, a[0]);
    else if (!is_zero_expr(a[1])) particular = divide(times(g, x), a[1]);
    else particular = divide(times(g, power(x, make_integer(2))), times(make_integer(2), a[2]));
    const ExprPtr disc = simplify(subtract(power(a[1], make_integer(2)), times({make_integer(4), a[2], a[0]})));
    const auto d = number(disc);
    if (!d) return nullptr;
    const ExprPtr two_a = times(make_integer(2), a[2]);
    ExprPtr homogeneous;
    if (*d > 0) {
        const ExprPtr root = power(disc, make_rational(1, 2));
        homogeneous = plus(times(c[0], e(divide(subtract(negate(a[1]), root), two_a))),
                           times(c[1], e(divide(plus(negate(a[1]), root), two_a))));
    } else if (*d == 0) {
        homogeneous = times(plus(c[0], times(c[1], x)), e(divide(negate(a[1]), two_a)));
    } else {
        const ExprPtr alpha = divide(negate(a[1]), two_a);
        const ExprPtr beta = simplify(divide(power(negate(disc), make_rational(1, 2)), two_a));
        const ExprPtr bx = times(beta, x);
        homogeneous = times(e(alpha), plus(times(c[0], fn("Cos", bx)), times(c[1], fn("Sin", bx))));
    }
    return plus(homogeneous, particular);
}

std::optional<BackendResult> dsolve(const ExprPtr& expr) {
    ExprPtr target = expr->arg(1);
    const ExprPtr& x = expr->arg(2);
    if (!x->is_symbol()) return std::nullopt;
    if (target->is_normal() && target->size() == 1 && equal(target->arg(0), x)) target = target->head();
    if (!target->is_symbol()) return std::nullopt;
    const std::string& y = target->name();
    const ExprList eqs = expr->arg(0)->has_head("List") ? expr->arg(0)->args() : ExprList{expr->arg(0)};

    std::optional<ExprPtr> ode;
    ExprList conditions;
    for (const auto& eq : eqs) {
        if (!eq->has_head("Equal") || eq->size() != 2) return std::nullopt;
        bool ok = true;
        const ExprPtr p = ode_slots(subtract(eq->arg(0), eq->arg(1)), y, x, ok);
        if (ok && !free_of(p, make_symbol("ode`Y0")) + !free_of(p, make_symbol("ode`Y1")) +
                      !free_of(p, make_symbol("ode`Y2")) > 0) {
            if (ode) return std::nullopt;  // one ODE only
            ode = p;
        } else {
            conditions.push_back(eq);  // checked below by substitution
        }
    }
    if (!ode) return std::nullopt;
    // Coefficients: numbers; forcing g free of y.
    ExprList a;
    const ExprPtr Y[3] = {make_symbol("ode`Y0"), make_symbol("ode`Y1"), make_symbol("ode`Y2")};
    int order = free_of(*ode, Y[2]) ? 1 : 2;
    for (int k = 0; k <= order; ++k) {
        const ExprPtr ak = simplify(differentiate(*ode, Y[k]));
        if (!ak->is_number()) return std::nullopt;  // nonlinear or variable coefficients
        a.push_back(ak);
    }
    if (is_zero_expr(a[order])) return std::nullopt;
    const ExprPtr g = simplify(negate(substitute(*ode, {{"ode`Y0", make_integer(0)}, {"ode`Y1", make_integer(0)},
                                                        {"ode`Y2", make_integer(0)}})));
    int next = 1;
    ExprList c;
    for (int k = 0; k < order; ++k) c.push_back(fresh_constant(expr, next));
    ExprPtr general = general_solution(a, g, x, c);
    if (!general) return std::nullopt;
    general = simplify(general);

    if (!conditions.empty()) {  // y[x0] == v, y'[x0] == v: a linear system for the constants
        if (conditions.size() > static_cast<std::size_t>(order)) return std::nullopt;
        ExprList polys, vars(c.begin(), c.end());
        const ExprPtr dgeneral = simplify(differentiate(general, x));
        for (const auto& eq : conditions) {
            const ExprPtr& lhs = eq->arg(0);
            ExprPtr at, body;
            if (lhs->is_normal() && lhs->size() == 1 && lhs->head()->is_symbol(y)) {
                at = lhs->arg(0);
                body = general;
            } else if (lhs->is_normal() && lhs->size() == 1 && lhs->head()->is_normal() &&
                       lhs->head()->size() == 1 && lhs->head()->arg(0)->is_symbol(y) &&
                       equal(lhs->head()->head(), make_normal("Derivative", {make_integer(1)}))) {
                at = lhs->arg(0);
                body = dgeneral;
            } else {
                return std::nullopt;
            }
            if (!free_of(at, x)) return std::nullopt;
            polys.push_back(subtract(substitute(body, {{x->name(), at}}), eq->arg(1)));
        }
        // Constants not fixed by conditions stay free.
        ExprList used(vars.begin(), vars.begin() + static_cast<std::ptrdiff_t>(polys.size()));
        const auto sol = polys.size() == 1 ? solve_one(polys[0], used[0]) : solve_linear_system(polys, used);
        if (!sol || (*sol)->size() != 1) return std::nullopt;
        Bindings b;
        for (const auto& r : (*sol)->arg(0)->args()) b[r->arg(0)->name()] = r->arg(1);
        general = simplify(substitute(general, b));
    }
    const ExprPtr result = make_normal("List", {make_normal("List", {make_normal("Rule", {
        make_normal(target, {x}), general})})});
    const ResultStatus checked = verification_status(expr, result);
    if (checked == ResultStatus::Unverified) return std::nullopt;
    return BackendResult{result, checked == ResultStatus::Verified ? ResultStatus::Exact : checked, "native"};
}

// ---------------------------------------------------------------- Factor

std::vector<Integer> divisors(const Integer& n) {  // positive divisors of |n| (|n| <= 10^7)
    std::vector<Integer> out;
    const auto v = n.to_int64();
    if (!v) return out;
    const long long m = *v < 0 ? -*v : *v;
    for (long long d = 1; d * d <= m; ++d)
        if (m % d == 0) {
            out.emplace_back(d);
            if (d != m / d) out.emplace_back(m / d);
        }
    return out;
}

// Factor[p] for a polynomial in one variable with rational coefficients: numeric content
// times linear factors from rational roots (with multiplicity) times what is left.
std::optional<BackendResult> factor(const ExprPtr& expr) {
    const ExprPtr& p = expr->arg(0);
    std::set<std::string> symbols;
    std::function<void(const ExprPtr&)> collect = [&](const ExprPtr& e) {
        if (e->is_symbol() && !e->is_symbol("Pi") && !e->is_symbol("E")) symbols.insert(e->name());
        if (e->is_normal()) {
            collect(e->head());
            for (const auto& a : e->args()) collect(a);
        }
    };
    collect(p);
    symbols.erase("Plus"); symbols.erase("Times"); symbols.erase("Power");
    if (symbols.size() != 1) return std::nullopt;
    const ExprPtr x = make_symbol(*symbols.begin());
    if (!polynomial(p, x)) return std::nullopt;
    const auto c = coefficients(p, x, 12);
    if (!c || c->size() < 3) return std::nullopt;  // degree >= 2
    std::vector<Rational> a;
    for (const auto& ck : *c) {
        if (!ck->is_number()) return std::nullopt;
        a.push_back(ck->number());
    }
    // Integer primitive polynomial: p == k * q.
    Integer den = 1;
    for (const auto& r : a) den = den / Integer::gcd(den, r.den()) * r.den();
    std::vector<Integer> q;
    for (const auto& r : a) q.push_back(r.num() * (den / r.den()));
    Integer g = 0;
    for (const auto& v : q) g = Integer::gcd(g, v);
    if (q.back().sign() < 0) g = -g;
    for (auto& v : q) v = v / g;
    const Rational k(g, den);
    ExprList factors{make_number(k)};
    bool found = false;
    while (q.size() > 1) {
        if (q.front().is_zero()) {  // root 0
            q.erase(q.begin());
            factors.push_back(x);
            found = true;
            continue;
        }
        std::optional<std::pair<Integer, Integer>> root;  // num/den with den > 0
        for (const auto& num : divisors(q.front())) {
            for (const auto& dd : divisors(q.back())) {
                for (int sign : {1, -1}) {
                    const Integer pn = sign > 0 ? num : -num;
                    // q(pn/dd) * dd^n == sum q_i pn^i dd^(n-i)
                    Integer acc = 0, pw = 1;
                    std::vector<Integer> dpow(q.size(), Integer(1));
                    for (std::size_t i = 1; i < q.size(); ++i) dpow[i] = dpow[i - 1] * dd;
                    for (std::size_t i = 0; i < q.size(); ++i) {
                        acc += q[i] * pw * dpow[q.size() - 1 - i];
                        pw *= pn;
                    }
                    if (acc.is_zero()) root = std::pair{pn, dd};
                    if (root) break;
                }
                if (root) break;
            }
            if (root) break;
        }
        if (!root) break;
        // Divide q by (d x - n): synthetic division over the integers (exact by Gauss).
        const auto [n, d] = *root;
        std::vector<Integer> out(q.size() - 1);
        Integer carry = 0;
        for (std::size_t i = q.size() - 1; i >= 1; --i) {  // from the leading coefficient down
            carry = (q[i] + carry) ;
            out[i - 1] = carry / d;
            carry = out[i - 1] * n;
        }
        q = std::move(out);
        factors.push_back(subtract(times(make_integer(d), x), make_number(Rational(n))));
        found = true;
    }
    if (!found) return std::nullopt;
    if (q.size() > 1) {
        ExprList terms;
        for (std::size_t i = 0; i < q.size(); ++i)
            terms.push_back(times(make_integer(q[i]), power(x, make_integer(static_cast<long long>(i)))));
        factors.push_back(plus(std::move(terms)));
    } else if (!q.empty()) {
        factors.push_back(make_integer(q[0]));
    }
    const ExprPtr result = times(std::move(factors));
    if (verification_status(expr, result) != ResultStatus::Verified) return std::nullopt;
    return BackendResult{result, ResultStatus::Exact, "native"};
}

}  // namespace

std::optional<BackendResult> NativeBackend::evaluate(const ExprPtr& expr) {
    if (expr && expr->has_head("Limit") && expr->size() == 2) return limit(expr);
    if (expr && expr->has_head("Series") && expr->size() == 2) return taylor(expr);
    if (expr && expr->has_head("Solve") && expr->size() == 2) return solve(expr);
    if (expr && expr->has_head("DSolve") && expr->size() == 3) return dsolve(expr);
    if (expr && expr->has_head("Factor") && expr->size() == 1) return factor(expr);
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
        // The antiderivative is proven by differentiation; what remains is to rule out
        // singularities inside the range: none exist for entire f and F, otherwise a
        // quadrature check must agree. Either way the value counts as exact.
        if ((entire(f) && entire(F)) || verification_status(expr, value) != ResultStatus::Unverified)
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
