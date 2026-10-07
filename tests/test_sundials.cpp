// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "backend/sundials/sundials_backend.h"
#include "symats/evaluator.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("SUNDIALS: NDSolve Backend Execution") {
    auto sundials = std::make_shared<SundialsBackend>();
    register_math_backend(sundials);

    Session session;
    ExprPtr expr = parse_text("NDSolve(Equal(D(y(x), x), -y(x)), y(x), {x, 0, 5})");
    ExprPtr result = session.eval(expr);

    CHECK(result != nullptr);
    CHECK(result->has_head("List"));
    CHECK_EQ(to_full_form(result), "List(Rule(y, InterpolatingFunction(List(0, 5), y)))");

    register_math_backend(nullptr);
}
