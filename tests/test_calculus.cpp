// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("Calculus: native D (differentiation)") {
    Context c;

    // D[x^3, x] -> 3*x^2
    CHECK_EQ(to_text(evaluate(parse_text("D[x^3, x]"), c)), "3*x^2");

    // D[Sin[x], x] -> Cos[x]
    CHECK_EQ(to_text(evaluate(parse_text("D[Sin[x], x]"), c)), "Cos[x]");

    // D[Cos[x], x] -> -Sin[x]
    CHECK_EQ(to_text(evaluate(parse_text("D[Cos[x], x]"), c)), "-Sin[x]");

    // D[Exp[x^2], x] -> 2*x*Exp[x^2]
    CHECK_EQ(to_text(evaluate(parse_text("D[Exp[x^2], x]"), c)), "2*x*Exp[x^2]");

    // D[Log[x], x] -> x^-1
    CHECK_EQ(to_text(evaluate(parse_text("D[Log[x], x]"), c)), "x^-1");

    // D[x^3 + 2*x, {x, 2}] -> 6*x
    CHECK_EQ(to_text(evaluate(parse_text("D[x^3 + 2*x, {x, 2}]"), c)), "6*x");

    // Partial derivatives: D[x^2 * y^3, x, y] -> 6*x*y^2
    CHECK_EQ(to_text(evaluate(parse_text("D[x^2 * y^3, x, y]"), c)), "6*x*y^2");
}

TEST_CASE("Calculus: native Expand") {
    Context c;

    // Expand[(x + 1)^2] -> 1 + 2*x + x^2
    CHECK_EQ(to_text(evaluate(parse_text("Expand[(x + 1)^2]"), c)), "1 + 2*x + x^2");

    // Expand[(x + y)*(x - y)] -> x^2 - y^2
    CHECK_EQ(to_text(evaluate(parse_text("Expand[(x + y)*(x - y)]"), c)), "x^2 - y^2");

    // Expand[(a + b)^3] -> a^3 + b^3 + 3*a*b^2 + 3*a^2*b
    CHECK_EQ(to_text(evaluate(parse_text("Expand[(a + b)^3]"), c)),
             "a^3 + b^3 + 3*a*b^2 + 3*a^2*b");
}
