// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac/giac_backend.h"
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
std::string F(const ExprPtr& e) { return e ? to_full_form(e) : std::string("<null>"); }
}  // namespace

TEST_CASE("GiacBackend: supports EXPR_SPEC heads") {
    GiacBackend giac;
    CHECK_EQ(giac.name(), std::string("giac"));
    CHECK(giac.supports("Integrate"));
    CHECK(giac.supports("Limit"));
    CHECK(giac.supports("Series"));
    CHECK(giac.supports("Solve"));
    CHECK(giac.supports("Factor"));
    CHECK(giac.supports("Simplify"));
    CHECK(giac.supports("DSolve"));
    CHECK(giac.supports("Det"));
    CHECK(giac.supports("Inverse"));
    CHECK(giac.supports("Transpose"));
    CHECK(giac.supports("Rank"));
    CHECK(giac.supports("Trace"));
    CHECK(giac.supports("RowReduce"));
    CHECK(giac.supports("CharPoly"));
    CHECK(giac.supports("MatrixExp"));
    CHECK(!giac.supports("Plus"));
}

TEST_CASE("GiacBackend: evaluation of Integrate, Simplify, and Linear Algebra") {
    GiacBackend giac;

    // Integrate x^2 dx -> 1/3 * x^3
    auto r1 = giac.evaluate(P("Integrate(x^2, x)"));
    CHECK(r1.has_value());
    CHECK_EQ(F(r1->value), std::string("Times(1/3, Power(x, 3))"));
    CHECK_EQ(r1->backend, std::string("giac"));

    // Simplify(Sin(x)^2 + Cos(x)^2) -> 1
    auto r2 = giac.evaluate(P("Simplify(Sin(x)^2 + Cos(x)^2)"));
    CHECK(r2.has_value());
    CHECK_EQ(F(r2->value), std::string("1"));

    // Det([[a, b], [c, d]]) -> a*d - b*c
    auto r3 = giac.evaluate(P("Det([[a, b], [c, d]])"));
    CHECK(r3.has_value());
    CHECK_EQ(F(r3->value), std::string("Plus(Times(a, d), Times(-1, b, c))"));

    // Trace([[1, 2], [3, 4]]) -> 5
    auto r4 = giac.evaluate(P("Trace([[1, 2], [3, 4]])"));
    CHECK(r4.has_value());
    CHECK_EQ(F(r4->value), std::string("5"));

    // Transpose([[1, 2], [3, 4]]) -> [[1, 3], [2, 4]]
    auto r5 = giac.evaluate(P("Transpose([[1, 2], [3, 4]])"));
    CHECK(r5.has_value());
    CHECK_EQ(F(r5->value), std::string("List(List(1, 3), List(2, 4))"));

    // Factor(x^2 - 1) -> (x - 1)*(x + 1)
    auto r6 = giac.evaluate(P("Factor(x^2 - 1)"));
    CHECK(r6.has_value());
    CHECK_EQ(F(r6->value), std::string("Times(Plus(-1, x), Plus(1, x))"));

    // Solve(x^2 - 4 == 0, x) -> {{x -> -2}, {x -> 2}}
    auto r7 = giac.evaluate(P("Solve(x^2 - 4 == 0, x)"));
    CHECK(r7.has_value());
    CHECK_EQ(F(r7->value), std::string("List(List(Rule(x, -2)), List(Rule(x, 2)))"));

    // Limit(Sin(x)/x, x, 0) -> 1
    auto r8 = giac.evaluate(P("Limit(Sin(x)/x, x, 0)"));
    CHECK(r8.has_value());
    CHECK_EQ(F(r8->value), std::string("1"));

    // DSolve(D(y(x), x) == y(x), y(x), x) -> {{y(x) -> C_1 * Exp(x)}}
    auto r9 = giac.evaluate(P("DSolve(D(y(x), x) == y(x), y(x), x)"));
    CHECK(r9.has_value());
    CHECK_EQ(F(r9->value), std::string("List(List(Rule(y(x), Times(C_1, Exp(x)))))"));
}

TEST_CASE("GiacBackend: integration with Context evaluator") {
    Context ctx;
    ctx.backends().add(std::make_shared<GiacBackend>());

    EvalResult res = evaluate_top(P("simplify(sin(x)^2 + cos(x)^2)"), ctx);
    CHECK_EQ(F(res.value), std::string("1"));
    CHECK(res.status == ResultStatus::Unverified);

    EvalResult res2 = evaluate_top(P("integrate(x^2, x)"), ctx);
    CHECK_EQ(F(res2.value), std::string("Times(1/3, Power(x, 3))"));
    CHECK(res2.status == ResultStatus::Unverified);
}
