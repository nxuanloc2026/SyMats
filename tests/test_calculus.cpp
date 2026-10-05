// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/pattern.h"
#include "symats/text.h"
#include "test.h"

#include <cmath>
#include <random>

using namespace symats;

namespace {
ExprPtr P(std::string_view source) { return parse_text(source); }
ExprPtr run(Context& ctx, std::string_view source) { return evaluate(P(source), ctx); }
void same(const ExprPtr& actual, std::string_view expected) {
    CHECK_EQ(to_full_form(actual), to_full_form(P(expected)));
}
double numeric(const ExprPtr& e, double x) {
    if (e->is_number()) return e->number().to_double();
    if (e->is_symbol("x")) return x;
    if (e->has_head("Plus")) {
        double sum = 0;
        for (const auto& a : e->args()) sum += numeric(a, x);
        return sum;
    }
    if (e->has_head("Times")) {
        double product = 1;
        for (const auto& a : e->args()) product *= numeric(a, x);
        return product;
    }
    if (e->has_head("Power")) return std::pow(numeric(e->arg(0), x), numeric(e->arg(1), x));
    if (e->has_head("Sin")) return std::sin(numeric(e->arg(0), x));
    if (e->has_head("Cos")) return std::cos(numeric(e->arg(0), x));
    if (e->has_head("Log")) return std::log(numeric(e->arg(0), x));
    throw std::invalid_argument("unsupported numeric spot-check expression");
}
}  // namespace

TEST_CASE("Calculus: native polynomial and partial derivatives") {
    Context ctx;
    same(run(ctx, "D[x^3 + 2*x*y, x]"), "3*x^2 + 2*y");
    same(run(ctx, "D[x^2*y, y]"), "x^2");
    same(run(ctx, "D[x^3, {x, 2}]"), "6*x");
    same(run(ctx, "D[x*y, x, y]"), "1");
    same(run(ctx, "D[7, x]"), "0");
    same(run(ctx, "D[y, x]"), "0");
}

TEST_CASE("Calculus: native chain and power rules") {
    Context ctx;
    same(run(ctx, "D[Sin[x^2], x]"), "2*x*Cos[x^2]");
    same(run(ctx, "D[Exp[3*x], x]"), "3*Exp[3*x]");
    same(run(ctx, "D[Log[x], x]"), "x^-1");
    same(run(ctx, "D[x^y, x]"), "y*x^(y-1)");
    same(run(ctx, "D[x^x, x]"), "x^x*(Log[x] + 1)");
    same(run(ctx, "D[Tanh[x], x]"), "Cosh[x]^-2");
    same(run(ctx, "D[Log[2, x], x]"), "1/(x*Log[2])");
    same(run(ctx, "D[Cot[x], x]"), "-Csc[x]^2");
    same(run(ctx, "D[Sec[x], x]"), "Sec[x]*Tan[x]");
    same(run(ctx, "D[Csc[x], x]"), "-Csc[x]*Cot[x]");
    same(run(ctx, "D[Tan[x], x]"), "Cos[x]^-2");
    same(run(ctx, "D[ArcSin[x], x]"), "(1-x^2)^(-1/2)");
    same(run(ctx, "D[ArcCos[x], x]"), "-(1-x^2)^(-1/2)");
    same(run(ctx, "D[ArcTan[x], x]"), "1/(1+x^2)");
    same(run(ctx, "D[Sinh[x], x]"), "Cosh[x]");
    same(run(ctx, "D[Cosh[x], x]"), "Sinh[x]");
    same(run(ctx, "D[Abs[x], x]"), "x/Abs[x]");
}

TEST_CASE("Calculus: unknown function derivatives remain symbolic") {
    Context ctx;
    ExprPtr x = make_symbol("x");
    ExprPtr f = make_symbol("f");
    ExprPtr first = P("Derivative[1][f][x]");
    CHECK(equal(run(ctx, "D[f[x], x]"), first));
    ExprPtr second = P("Derivative[2][f][x]");
    CHECK(equal(run(ctx, "D[f[x], {x, 2}]"), second));
    CHECK(equal(run(ctx, "D[f[x,y],x]"), P("Derivative[{1,0}][f][x,y]")));
    CHECK(equal(run(ctx, "D[f[x,y],{x,2}]"), P("Derivative[{2,0}][f][x,y]")));

    evaluate(make_normal("SetDelayed", {
        make_normal("sq", {pat("u")}), P("u^2")}), ctx);
    same(run(ctx, "D[sq[x], x]"), "2*x");
    ExprPtr applied = P("Derivative[1][sq][3]");
    same(evaluate(applied, ctx), "6");
}

TEST_CASE("Calculus: Expand distributes products and powers") {
    Context ctx;
    same(run(ctx, "Expand[(x+1)^3]"), "x^3 + 3*x^2 + 3*x + 1");
    same(run(ctx, "Expand[(x+y)*(x-y)]"), "x^2 - y^2");
    same(run(ctx, "Expand[(x+y)^0]"), "1");
    same(run(ctx, "Expand[Sin[(x+1)^2]]"), "Sin[x^2+2*x+1]");
    same(run(ctx, "Expand[(x+y)^-1]"), "(x+y)^-1");
}

TEST_CASE("Calculus: derivatives agree with finite differences at sample points") {
    Context ctx;
    std::mt19937 rng(10010);
    std::uniform_real_distribution<double> points(0.4, 2.2);
    for (std::string_view source : {"x^3 + 2*x", "Sin[x^2]", "x^x"}) {
        ExprPtr f = P(source);
        ExprPtr df = evaluate(make_normal("D", {f, make_symbol("x")}), ctx);
        for (int sample = 0; sample < 12; ++sample) {
            const double x = points(rng);
            const double h = 1e-6;
            const double finite = (numeric(f, x + h) - numeric(f, x - h)) / (2 * h);
            CHECK(std::abs(numeric(df, x) - finite) < 1e-5);
        }
    }
}
