// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.h"
#include "test.h"
#include "symats/text.h"

#include <cmath>

TEST_CASE("OdeintBackend NDSolve and special functions") {
    symats::OdeintBackend backend;

    // Test Gamma[5] = 4! = 24
    auto gamma_expr = symats::parse_text("Gamma[5]");
    auto gamma_res = backend.evaluate(gamma_expr);
    CHECK(gamma_res.has_value());
    CHECK(gamma_res->status == symats::ResultStatus::Numeric);
    CHECK(std::abs(gamma_res->value->number().to_double() - 24.0) < 1e-4);

    // Test Erf[0] = 0
    auto erf_expr = symats::parse_text("Erf[0]");
    auto erf_res = backend.evaluate(erf_expr);
    CHECK(erf_res.has_value());
    CHECK(std::abs(erf_res->value->number().to_double() - 0.0) < 1e-4);

    // Test NDSolve[{Derivative[1][y][t] == y[t], y[0] == 1}, y, {t, 0, 1}]
    auto nds_expr = symats::parse_text("NDSolve[{Derivative[1][y][t] == y[t], y[0] == 1}, y, {t, 0, 1}]");
    auto nds_res = backend.evaluate(nds_expr);
    CHECK(nds_res.has_value());
    CHECK(nds_res->status == symats::ResultStatus::Numeric);
    CHECK(nds_res->value->has_head("InterpolatingFunction"));
}
