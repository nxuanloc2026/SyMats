// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac_backend.h"
#include "symats/text.h"
#include "symats/calculus.h"
#include "symats/eval.h"

#include "test.h"


using namespace symats;
namespace {
ExprPtr run(std::string_view text) {
    GiacBackend backend;
    auto result = backend.evaluate(parse_text(text));
    if (!result) {
        symats_test::report(__FILE__, __LINE__, "backend declined " + std::string(text));
        return nullptr;
    }
    CHECK(result->status == ResultStatus::Verified ||
          result->status == ResultStatus::Unverified);
    CHECK_EQ(result->backend, "giac");
    return result->value;
}
void check(std::string_view input, std::string_view expected) {
    auto value = run(input);
    if (value && !equal(value, parse_text(expected)))
        symats_test::report(__FILE__, __LINE__, std::string(input) + " produced " +
            to_text(value) + "; expected " + std::string(expected));
}
}

TEST_CASE("Giac bridge roundtrips Tier 1 expression trees") {
    constexpr std::string_view cases[] = {
        "123456789012345678901234567890", "-7/13", "{x, Pi, pi, E, I, Infinity, True, False}",
        "2*x^2 + Sin[x]", "f[x, {a, b}]", "f[]", "Derivative[2][y][t]", "D[u[x,t],{x,2}]",
        "f[x_Integer] := x^2", "a = 3", "ReplaceAll[x,x->2]", "And[x<3,Not[False]]",
        "{{a,b},{c,d}}", "Integrate[x, {x,0,1}]", "Limit[Sin[x]/x,x->0]",
        "Series[Exp[x],{x,0,3}]", "Solve[x==1,x]", "DSolve[Derivative[1][y][x]==y[x],y[x],x]",
        "Plot[Sin[x],{x,0,Pi}]", "Animate[Plot[x*t,{x,0,1}],{t,0,2}]",
        "Sum[k,{k,1,3}]", "Product[k,{k,1,3}]", "Dot[{a,b},{c,d}]",
        "Log[2,x]", "Transpose[{{x}}]", "MatrixExp[{{x}}]", "Out[3]",
    };
    for (const auto text : cases) {
        const auto expr = parse_text(text);
        const auto result = giac_roundtrip(expr);
        CHECK(result.has_value());
        if (result) CHECK_EQ(to_full_form(*result), to_full_form(expr));
    }
    CHECK(!giac_roundtrip(nullptr));
}

TEST_CASE("Giac bridge calculates exact algebra calculus and preserves names") {
    check("Factor[x^2-1]", "(x-1)*(x+1)");
    check("Simplify[Sin[x]^2+Cos[x]^2]", "1");
    check("Simplify[Sin[Pi]]", "0");
    check("Simplify[Sin[pi]]", "Sin[pi]");
    check("Simplify[I^2]", "-1");
    check("Integrate[x,x]", "x^2/2");
    check("Integrate[Sin[x],x]", "-Cos[x]");
    check("Integrate[x^2,{x,0,1}]", "1/3");
    check("Integrate[1,{x,0,y},{y,0,1}]", "1/2");
    check("Limit[Sin[x]/x,x->0]", "1");
    check("Series[Exp[x],{x,0,3}]", "SeriesData[1+x+x^2/2+x^3/6,{x,0,3}]");
    check("Together[1/x+1/(x+1)]", "(2*x+1)/(x^2+x)");
}

TEST_CASE("Giac bridge shapes algebraic and differential solutions as rules") {
    check("Solve[x^2==4,x]", "{{x->-2},{x->2}}");
    check("Solve[{x+y==3,x-y==1},{x,y}]", "{{x->2,y->1}}");
    check("DSolve[Derivative[1][y][x]==y[x],y[x],x]", "{{y[x]->C1*Exp[x]}}");
    check("DSolve[{Derivative[1][y][x]==y[x],y[0]==1},y[x],x]", "{{y[x]->Exp[x]}}");
    check("DSolve[{Derivative[2][y][x]+y[x]==0,y[0]==0,Derivative[1][y][0]==1},y[x],x]", "{{y[x]->Sin[x]}}");
    check("DSolve[{Derivative[1][u][t]==v[t],Derivative[1][v][t]==-u[t],u[0]==0,v[0]==1},{u[t],v[t]},t]",
          "{{u[t]->Sin[t],v[t]->Cos[t]}}");
    check("DSolve[Derivative[1][y][x]==C1,y[x],x]", "{{y[x]->C1*x+C2}}");
    check("DSolve[{D[u[t],t]==v[t],D[v[t],t]==0,u[0]==0,v[0]==1},{u[t],v[t]},t]",
          "{{u[t]->t,v[t]->1}}");
    check("DSolve[{D[u[t],t]==t,D[v[t],t]==u[t],u[0]==0,v[0]==0},{u[t],v[t]},t]",
          "{{u[t]->t^2/2,v[t]->t^3/6}}");
}

TEST_CASE("Giac bridge performs exact linear algebra") {
    check("Det[{{a,b},{c,d}}]", "a*d-b*c");
    check("Inverse[{{1,2},{3,4}}]", "{{-2,1},{3/2,-1/2}}");
    check("Rank[{{1,2},{3,4}}]", "2");
    check("Trace[{{1,2},{3,4}}]", "5");
    check("RowReduce[{{1,2},{3,4}}]", "{{1,0},{0,1}}");
    check("CharPoly[{{1,2},{3,4}},z]", "z^2-5*z-2");
    check("Transpose[{{1,2},{3,4}}]", "{{1,3},{2,4}}");
    check("LinearSolve[{{1,2},{3,4}},{5,11}]", "{1,2}");
    check("MatrixExp[{{0,1},{0,0}}]", "{{1,1},{0,1}}");
    check("Dot[{1,2},{3,4}]", "11");
    check("Dot[{{1,2},{3,4}},{1,2}]", "{5,11}");
    check("Dot[{1,2},{{1,2},{3,4}}]", "{7,10}");
    const auto eigenvalues = run("Eigenvalues[{{1,0},{0,2}}]");
    const auto eigenvectors = run("Eigenvectors[{{1,0},{0,2}}]");
    if (eigenvalues && eigenvectors) {
        CHECK(eigenvalues->size() == 2 && eigenvectors->size() == 2);
        for (std::size_t i = 0; i < 2; ++i) {
            const auto& vector = eigenvectors->arg(i);
            CHECK(vector->size() == 2);
            CHECK(!equal(vector, parse_text("{0,0}")));
            CHECK(equal(vector->arg(0), times(eigenvalues->arg(i), vector->arg(0))));
            CHECK(equal(times(make_integer(2), vector->arg(1)),
                        times(eigenvalues->arg(i), vector->arg(1))));
        }

    }
}

TEST_CASE("Giac bridge marks natively checked results verified") {
    GiacBackend backend;
    const auto integral = backend.evaluate(parse_text("Integrate[x^2,x]"));
    const auto solve = backend.evaluate(parse_text("Solve[x^2==4,x]"));
    const auto inverse = backend.evaluate(parse_text("Inverse[{{1,2},{3,4}}]"));
    CHECK(integral && integral->status == ResultStatus::Verified);
    CHECK(solve && solve->status == ResultStatus::Verified);
    CHECK(inverse && inverse->status == ResultStatus::Verified);
}

TEST_CASE("Giac bridge declines malformed and unsupported requests") {
    GiacBackend backend;
    CHECK(!backend.supports("Plot"));
    CHECK(!backend.supports("D"));
    CHECK(!backend.evaluate(nullptr));
    constexpr std::string_view cases[] = {
        "Factor[]", "Limit[x,x]", "Integrate[x,{x,0}]", "Series[Exp[x],{x,0,-1}]",
        "Solve[x==1,3]", "Solve[x==1,{x,x}]", "Det[{{1,2},{3}}]", "Inverse[{{0}}]",
        "DSolve[u[x,t]==0,u[x,t],{x,t}]", "Plot[x,{x,0,1}]",
        "Dot[{{1},{2,3}},{1,2}]", "Dot[{1},{1,2}]",
        "DSolve[{D[u[t],t]==u[t]^2,D[v[t],t]==0},{u[t],v[t]},t]",
    };
    for (const auto text : cases) CHECK(!backend.evaluate(parse_text(text)));
}

TEST_CASE("Giac registers with the evaluator and keeps backend status") {
    Context context;
    context.backends().add(std::make_shared<GiacBackend>());
    const auto result = evaluate_top(parse_text("Integrate[x^2,{x,0,1}]"),context);
    CHECK(equal(result.value,parse_text("1/3")));
    CHECK(result.status == ResultStatus::Unverified);
}
