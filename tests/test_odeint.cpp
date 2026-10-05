// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.hpp"
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("OdeintBackend: NDSolve and special functions") {
    Context ctx;
    auto backend = std::make_shared<OdeintBackend>();
    ctx.backends().add(backend);

    // Test Special Functions: Gamma(5) = 24
    ExprPtr gamma5 = parse_text("Gamma[5]");
    auto res_gamma = ctx.backends().try_evaluate(gamma5);
    CHECK(res_gamma.has_value());
    CHECK_EQ(to_string(res_gamma->status), std::string("numeric"));
    CHECK_EQ(res_gamma->value->number().to_double(), 24.0);

    // Test Erf(0) = 0
    ExprPtr erf0 = parse_text("Erf[0]");
    auto res_erf = ctx.backends().try_evaluate(erf0);
    CHECK(res_erf.has_value());
    CHECK_EQ(res_erf->value->number().to_double(), 0.0);

    // Test NDSolve: D[y[x], x] == -y[x], y[0] == 1 from x=0 to 1
    // Exact solution: y(x) = Exp[-x]. At x=1, y(1) = 1/e ≈ 0.367879
    ExprPtr ode_expr = parse_text("NDSolve[{D[y[x], x] == -y[x], y[0] == 1}, y, {x, 0, 1}]");
    auto res_ode = ctx.backends().try_evaluate(ode_expr);
    CHECK(res_ode.has_value());
    CHECK_EQ(to_string(res_ode->status), std::string("numeric"));
    CHECK_EQ(res_ode->backend, std::string("odeint"));

    // Check that output is List[List[Rule[y, InterpolatingFunction[...]]]]
    const auto& val = res_ode->value;
    CHECK(val->has_head("List"));
    CHECK_EQ(val->size(), std::size_t(1));
    const auto& rule_list = val->arg(0);
    CHECK(rule_list->has_head("List"));
    CHECK_EQ(rule_list->size(), std::size_t(1));
    const auto& rule = rule_list->arg(0);
    CHECK(rule->has_head("Rule"));
    CHECK_EQ(rule->arg(0)->name(), std::string("y"));
    CHECK(rule->arg(1)->has_head("InterpolatingFunction"));
}
