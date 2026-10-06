// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/calculus.h"

#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
std::string run(Context& c, std::string_view s) { return to_full_form(evaluate(P(s), c)); }
}  // namespace

TEST_CASE("Calculus: native differentiation D") {
    Context c;
    // Constants and power rule
    CHECK_EQ(run(c, "D[5, x]"), "0");
    CHECK_EQ(run(c, "D[x, x]"), "1");
    CHECK_EQ(run(c, "D[y, x]"), "0");
    CHECK_EQ(run(c, "D[x^3, x]"), "Times(3, Power(x, 2))");
    CHECK_EQ(run(c, "D[x^4 + 3*x^2, x]"), "Plus(Times(6, x), Times(4, Power(x, 3)))");

    // Elementary functions and chain rule
    CHECK_EQ(run(c, "D[Sin[x], x]"), "Cos(x)");
    CHECK_EQ(run(c, "D[Cos[x], x]"), "Times(-1, Sin(x))");
    CHECK_EQ(run(c, "D[Sin[x^2], x]"), "Times(2, x, Cos(Power(x, 2)))");
    CHECK_EQ(run(c, "D[Exp[x^2], x]"), "Times(2, x, Exp(Power(x, 2)))");
    CHECK_EQ(run(c, "D[Log[x], x]"), "Power(x, -1)");

    // Product rule
    CHECK_EQ(run(c, "D[x * Sin[x], x]"), "Plus(Sin(x), Times(x, Cos(x)))");

    // Higher order derivatives D[f, {x, 2}]
    CHECK_EQ(run(c, "D[x^4, {x, 2}]"), "Times(12, Power(x, 2))");

    // Multivariable derivatives D[f, x, y]
    CHECK_EQ(run(c, "D[x^2 * y^3, x, y]"), "Times(6, x, Power(y, 2))");
}

TEST_CASE("Calculus: native expansion Expand") {
    Context c;
    CHECK_EQ(run(c, "Expand[(x + 1)^2]"), "Plus(1, Times(2, x), Power(x, 2))");
    CHECK_EQ(run(c, "Expand[(a + b)*(a - b)]"), "Plus(Power(a, 2), Times(-1, Power(b, 2)))");
    CHECK_EQ(run(c, "Expand[(a + b)^3]"), "Plus(Power(a, 3), Power(b, 3), Times(3, a, Power(b, 2)), Times(3, Power(a, 2), b))");
}
