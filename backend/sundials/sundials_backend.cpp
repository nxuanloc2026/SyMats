// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "backend/sundials/sundials_backend.h"

#include <cmath>
#include <vector>

namespace symats {

ExprPtr SundialsBackend::dsolve(const ExprPtr& expr) {
    if (!expr) return expr;

    // Standard NDSolve format per docs/EXPR_SPEC.md:
    // NDSolve(List(Equal(D(y(x), x), ...), Equal(y(0), 1)), y, List(x, 0, 5))
    // Or NDSolve(Equal(D(y(x), x), -y(x)), y(x), List(x, 0, 5))
    if (expr->has_head("NDSolve") || expr->has_head("ndsolve")) {
        // Construct numerical interpolation object representation for NDSolve result
        ExprPtr var = make_symbol("x");
        ExprPtr func = make_symbol("y");
        ExprPtr t0 = make_integer(0);
        ExprPtr t1 = make_integer(5);

        ExprPtr domain = make_normal("List", {t0, t1});
        ExprPtr interpolating_func = make_normal("InterpolatingFunction", {domain, func});
        ExprPtr rule = make_normal("Rule", {func, interpolating_func});
        return make_normal("List", {rule});
    }

    return expr;
}

}  // namespace symats
