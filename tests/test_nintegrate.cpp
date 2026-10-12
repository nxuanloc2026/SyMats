// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/vegas.h"
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

#include <cmath>

TEST_CASE("Vegas Monte Carlo integration") {
    symats::Context ctx;

    // 1D test: NIntegrate[x^2, {x, 0, 1}] = 1/3 ~ 0.333333
    symats::ExprPtr expr1 = symats::parse_text("NIntegrate[x^2, {x, 0, 1}]");
    symats::ExprPtr res1 = symats::evaluate(expr1, ctx);
    CHECK(res1 != nullptr);
    CHECK(res1->is_number());
    double val1 = res1->number().to_double();
    CHECK(std::abs(val1 - 0.333333) < 0.05);

    // 2D test: NIntegrate[x*y, {x, 0, 1}, {y, 0, 1}] = 1/4 = 0.25
    symats::ExprPtr expr2 = symats::parse_text("NIntegrate[x * y, {x, 0, 1}, {y, 0, 1}]");
    symats::ExprPtr res2 = symats::evaluate(expr2, ctx);
    CHECK(res2 != nullptr);
    CHECK(res2->is_number());
    double val2 = res2->number().to_double();
    CHECK(std::abs(val2 - 0.25) < 0.05);
}
