// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
bool same(const char* input, const char* expected) {
    Context ctx;
    return equal(evaluate(parse_text(input), ctx), evaluate(parse_text(expected), ctx));
}
}  // namespace

TEST_CASE("Elementary functions: exact values at 0, Log[1], Log[E]") {
    CHECK(same("Sin[0]", "0"));
    CHECK(same("Cos[0]", "1"));
    CHECK(same("Exp[0]", "1"));
    CHECK(same("Tanh[0] + ArcTan[0] + ArcSinh[0]", "0"));
    CHECK(same("Log[1]", "0"));
    CHECK(same("Log[E]", "1"));
    CHECK(same("Sin[{0, x}]", "{0, Sin[x]}"));
    CHECK(!same("Sin[1]", "0"));
    CHECK(!same("Log[2]", "1"));
}

TEST_CASE("D: E^x and inverse hyperbolic functions") {
    CHECK(same("D[E^x, x]", "E^x"));
    CHECK(same("D[E^(2*x), x]", "2*E^(2*x)"));
    CHECK(same("D[ArcSinh[x], x]", "(1 + x^2)^(-1/2)"));
    CHECK(same("D[ArcCosh[x], x]", "(-1 + x^2)^(-1/2)"));
    CHECK(same("D[ArcTanh[x], x]", "(1 - x^2)^-1"));
}
