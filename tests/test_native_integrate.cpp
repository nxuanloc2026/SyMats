// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/native_backend.h"

#include <memory>

#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
struct Run {
    Context ctx;
    Run() { ctx.backends().add(std::make_shared<NativeBackend>()); }
    EvalResult operator()(const char* text) { return evaluate_top(parse_text(text), ctx); }
    bool same(const char* input, const char* expected) {
        return equal((*this)(input).value, evaluate(parse_text(expected), ctx));
    }
};

// D[F, x] == f exactly, for the antiderivative the backend returns.
bool antiderivative_ok(const char* f) {
    Run run;
    const auto r = run((std::string("Integrate[") + f + ", x]").c_str());
    if (r.value->has_head("Integrate") || r.status != ResultStatus::Exact) return false;
    Context ctx;
    const auto diff = evaluate(expand(subtract(differentiate(r.value, make_symbol("x")), parse_text(f))), ctx);
    return diff->is_integer() && diff->integer().is_zero();
}
}  // namespace

TEST_CASE("Native Integrate: table forms, linearity and expansion") {
    for (const char* f : {"x^3", "3*x^2 + 2*x + 1", "1/x", "x^(-2)", "Sqrt[x]", "Exp[x]", "E^(2*x)",
                          "Sin[3*x]", "Cos[x/2]", "Sinh[x]", "Cosh[2*x + 1]", "2^x", "(2*x + 1)^5",
                          "1/(3*x - 1)", "a*Sin[x] + b", "(x + 1)*(x - 2)", "(x + 1)^2",
                          "x*Sin[x]", "x^2*Exp[3*x]", "(x + 1)*Cos[2*x]", "x^3*Sinh[x]", "x*2^x"})
        CHECK(antiderivative_ok(f));
    Run run;
    CHECK(run.same("Integrate[1/(1 + x^2), x]", "ArcTan[x]"));
    CHECK(run.same("Integrate[(1 - x^2)^(-1/2), x]", "ArcSin[x]"));
    CHECK(run.same("Integrate[Log[x], x]", "x*Log[x] - x"));
    CHECK(run.same("Integrate[5, x]", "5*x"));
    CHECK(run.same("Integrate[Sec[x]^2, x]", "Tan[x]"));
    CHECK(run.same("Integrate[Csc[2*x]^2, x]", "-Cot[2*x]/2"));
}

TEST_CASE("Native Integrate: declines what it cannot do") {
    Run run;
    CHECK(run("Integrate[Exp[x^2], x]").value->has_head("Integrate"));
    CHECK(run("Integrate[Log[x]^2, x]").value->has_head("Integrate"));  // not in the table
    CHECK(run("Integrate[f[x], x]").value->has_head("Integrate"));
}

TEST_CASE("Native Integrate: definite integrals") {
    Run run;
    auto r = run("Integrate[x^2, {x, 0, 1}]");
    CHECK(equal(r.value, parse_text("1/3")));
    CHECK(r.status == ResultStatus::Exact);
    CHECK(run.same("Integrate[Exp[x], {x, 0, 1}]", "E - 1"));
    CHECK(run.same("Integrate[a*x, {x, 0, b}]", "a*b^2/2"));
    // Highly oscillatory but entire: the closed form needs no quadrature check.
    CHECK(run.same("Integrate[Sin[1000*x], {x, 0, 1000}]", "1/1000 - Cos[1000000]/1000"));
    CHECK(run.same("Integrate[Exp[2*x] + x^3, {x, -1, 1}]", "Exp[2]/2 - Exp[-2]/2"));
    // No closed form here, or a singularity inside the range: numeric (NIntegrate) instead.
    r = run("Integrate[Exp[x^2], {x, 0, 1}]");
    CHECK(r.value->has_head("NIntegrate"));
    CHECK(r.status == ResultStatus::Numeric);
    CHECK(run("Integrate[1/x, {x, -1, 1}]").value->has_head("NIntegrate"));
    CHECK(run("Integrate[Exp[a*x^2], {x, 0, 1}]").value->has_head("Integrate"));  // parameter
}

TEST_CASE("Native Series: Taylor polynomials as SeriesData") {
    Run run;
    CHECK(run.same("Series[Exp[x], {x, 0, 3}]", "SeriesData[1 + x + x^2/2 + x^3/6, {x, 0, 3}]"));
    CHECK(run.same("Series[Sin[x], {x, 0, 5}]", "SeriesData[x - x^3/6 + x^5/120, {x, 0, 5}]"));
    CHECK(run.same("Series[Log[x], {x, 1, 2}]", "SeriesData[(x - 1) - (x - 1)^2/2, {x, 1, 2}]"));
    CHECK(run.same("Series[1/(1 - x), {x, 0, 3}]", "SeriesData[1 + x + x^2 + x^3, {x, 0, 3}]"));
    CHECK(run.same("Series[x^2 + 3, {x, 0, 0}]", "SeriesData[3, {x, 0, 0}]"));
    CHECK(run("Series[Exp[x], {x, 0, 3}]").status == ResultStatus::Exact);
    // Singular points need Laurent/Puiseux series: declined.
    CHECK(run("Series[1/x, {x, 0, 2}]").value->has_head("Series"));
    CHECK(run("Series[Log[x], {x, 0, 2}]").value->has_head("Series"));
    CHECK(run("Series[Sqrt[x], {x, 0, 2}]").value->has_head("Series"));
    CHECK(run("Series[Exp[x], {x, 0, 50}]").value->has_head("Series"));  // left to Giac
}

TEST_CASE("Native Solve: linear, quadratic and linear systems, checked by substitution") {
    Run run;
    CHECK(run.same("Solve[2*x + 3 == 7, x]", "{{x -> 2}}"));
    CHECK(run.same("Solve[a*x == b, x]", "{{x -> b/a}}"));
    CHECK(run.same("Solve[x^2 == 4, x]", "{{x -> -2}, {x -> 2}}"));
    CHECK(run.same("Solve[x^2 - 2*x + 1 == 0, x]", "{{x -> 1}}"));
    auto r = run("Solve[x^2 == 2, x]");
    CHECK(r.value->size() == 2 && r.status == ResultStatus::Exact);
    CHECK(run.same("Solve[{x + y == 3, x - y == 1}, {x, y}]", "{{x -> 2, y -> 1}}"));
    CHECK(run.same("Solve[{2*x + y == 1, x + 3*y + z == 2, y + z == 0}, {x, y, z}]",
                   "{{x -> 0, y -> 1, z -> -1}}"));
    r = run("Solve[{a*x + y == 1, x - y == 0}, {x, y}]");  // x == y == 1/(1 + a), in some form
    CHECK(r.value->has_head("List") && r.value->size() == 1 && r.value->arg(0)->size() == 2);
    CHECK(r.status == ResultStatus::Numeric);  // Cramer form only confirmed numerically
    // Complex roots stay in radical form: x -> -(1/2)*(-4)^(1/2), x -> (1/2)*(-4)^(1/2).
    CHECK(run("Solve[x^2 == -1, x]").value->size() == 2);
    // Not handled natively: cubic, nonlinear system, singular system.
    CHECK(run("Solve[x^3 == 2, x]").value->has_head("Solve"));
    CHECK(run("Solve[{x*y == 1, x + y == 2}, {x, y}]").value->has_head("Solve"));
    CHECK(run("Solve[{x + y == 1, 2*x + 2*y == 2}, {x, y}]").value->has_head("Solve"));
}

TEST_CASE("Native Limit: substitution and L'Hopital, checked numerically") {
    Run run;
    CHECK(run.same("Limit[x^2 + 1, x -> 2]", "5"));
    CHECK(run.same("Limit[Sin[x]/x, x -> 0]", "1"));
    CHECK(run.same("Limit[(1 - Cos[x])/x^2, x -> 0]", "1/2"));
    CHECK(run.same("Limit[(x^2 - 1)/(x - 1), x -> 1]", "2"));
    CHECK(run.same("Limit[(Exp[x] - 1)/x, x -> 0]", "1"));
    CHECK(run("Limit[Sin[x]/x, x -> 0]").status == ResultStatus::Exact);
    // No finite two-sided limit, or not numeric: declined.
    CHECK(run("Limit[1/x, x -> 0]").value->has_head("Limit"));
    CHECK(run("Limit[Abs[x]/x, x -> 0]").value->has_head("Limit"));
    CHECK(run("Limit[Sin[a*x]/x, x -> 0]").value->has_head("Limit"));
    CHECK(run("Limit[x, x -> Infinity]").value->has_head("Limit"));
}

TEST_CASE("Native DSolve: linear constant-coefficient ODEs, verified by substitution") {
    Run run;
    CHECK(run.same("DSolve[y'[x] == y[x], y[x], x]", "{{y[x] -> C1*Exp[x]}}"));
    CHECK(run.same("DSolve[{y'[x] == y[x], y[0] == 2}, y[x], x]", "{{y[x] -> 2*Exp[x]}}"));
    CHECK(run.same("DSolve[{y''[x] + y[x] == 0, y[0] == 0, y'[0] == 1}, y[x], x]", "{{y[x] -> Sin[x]}}"));
    auto r = run("DSolve[y''[x] - 3*y'[x] + 2*y[x] == 0, y, x]");  // roots 1 and 2
    CHECK(r.value->has_head("List") && r.status == ResultStatus::Exact);
    r = run("DSolve[y''[x] + 2*y'[x] + y[x] == 0, y[x], x]");  // double root -1
    CHECK(r.value->has_head("List") && r.status == ResultStatus::Exact);
    r = run("DSolve[{y''[x] + 2*y'[x] + 5*y[x] == 10, y[0] == 2, y'[0] == 0}, y[x], x]");  // damped, forced
    CHECK(r.value->has_head("List"));
    r = run("DSolve[y'[x] + 2*y[x] == x, y[x], x]");  // first order with forcing
    CHECK(r.value->has_head("List") && r.status != ResultStatus::Unverified);
    CHECK(run.same("DSolve[y'[x] == 3, y[x], x]", "{{y[x] -> C1 + 3*x}}"));
    // A constant name already in use is skipped.
    CHECK(run.same("DSolve[y'[x] == C1*y[x], y[x], x]", "{{y[x] -> C2*Exp[C1*x]}}") ||
          run("DSolve[y'[x] == C1*y[x], y[x], x]").value->has_head("DSolve"));
    // Not handled natively: nonlinear, variable coefficients, non-constant forcing of order 2.
    CHECK(run("DSolve[y'[x] == y[x]^2, y[x], x]").value->has_head("DSolve"));
    CHECK(run("DSolve[y'[x] == x*y[x], y[x], x]").value->has_head("DSolve"));
    CHECK(run("DSolve[y''[x] + y[x] == Sin[x], y[x], x]").value->has_head("DSolve"));
}

TEST_CASE("Native Factor: rational roots of univariate polynomials, checked by Expand") {
    Run run;
    CHECK(run.same("Factor[x^2 - 1]", "(x - 1)*(x + 1)"));
    CHECK(run.same("Factor[x^3 - 6*x^2 + 11*x - 6]", "(x - 1)*(x - 2)*(x - 3)"));
    CHECK(run.same("Factor[2*x^2 + 3*x + 1]", "(2*x + 1)*(x + 1)"));
    CHECK(run.same("Factor[x^3 - x]", "x*(x - 1)*(x + 1)"));
    CHECK(run.same("Factor[(x + 1)^3]", "(x + 1)^3"));                 // multiplicity
    CHECK(run.same("Factor[x^2/2 - 1/2]", "(x - 1)*(x + 1)/2"));         // rational content
    CHECK(run.same("Factor[x^3 + x^2 + x + 1]", "(x + 1)*(1 + x^2)"));   // irreducible rest kept
    CHECK(run.same("Factor[-x^2 + 4]", "-(x - 2)*(x + 2)"));
    CHECK(run("Factor[x^3 - 6*x^2 + 11*x - 6]").status == ResultStatus::Exact);
    // Nothing to factor over the rationals, or not univariate: left alone.
    CHECK(run("Factor[x^2 + 1]").value->has_head("Factor"));
    CHECK(run("Factor[x^2 - 2]").value->has_head("Factor"));
    CHECK(run("Factor[x^2 - y^2]").value->has_head("Factor"));
}
