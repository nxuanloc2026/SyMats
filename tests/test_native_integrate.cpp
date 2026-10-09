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
                          "1/(3*x - 1)", "a*Sin[x] + b", "(x + 1)*(x - 2)", "(x + 1)^2"})
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
    CHECK(run("Integrate[x*Sin[x], x]").value->has_head("Integrate"));  // needs integration by parts
    CHECK(run("Integrate[f[x], x]").value->has_head("Integrate"));
}

TEST_CASE("Native Integrate: definite integrals") {
    Run run;
    auto r = run("Integrate[x^2, {x, 0, 1}]");
    CHECK(equal(r.value, parse_text("1/3")));
    CHECK(r.status == ResultStatus::Exact);
    CHECK(run.same("Integrate[Exp[x], {x, 0, 1}]", "E - 1"));
    CHECK(run.same("Integrate[a*x, {x, 0, b}]", "a*b^2/2"));
    // No closed form here, or a singularity inside the range: numeric (NIntegrate) instead.
    r = run("Integrate[Exp[x^2], {x, 0, 1}]");
    CHECK(r.value->has_head("NIntegrate"));
    CHECK(r.status == ResultStatus::Numeric);
    CHECK(run("Integrate[1/x, {x, -1, 1}]").value->has_head("NIntegrate"));
    CHECK(run("Integrate[Exp[a*x^2], {x, 0, 1}]").value->has_head("Integrate"));  // parameter
}
