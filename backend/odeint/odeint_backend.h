// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "symats/backend.h"

namespace symats {

// Boost.Odeint solver for explicit initial value problems:
//   NDSolve[{y''[t] == f[t, y[t], y'[t], ...], y[t0] == a, y'[t0] == b, ...}, y, {t, t0, t1}]
//   NDSolve[{x'[t] == ..., y'[t] == ..., x[t0] == ..., y[t0] == ...}, {x, y}, {t, t0, t1}]
// Any order and any number of unknown functions; the right-hand sides may use the
// elementary functions and Boost.Math special functions (Gamma, Erf, BesselJ, ...).
//
// It integrates with Dormand-Prince 5(4) and switches to Rosenbrock 4 (stiff,
// symbolic Jacobian via native D) when the explicit method exceeds its step budget.
// Result for a single function `y`: InterpolatingFunction[{t0, t1}, {{t, y}, ...}],
// the same shape as the SUNDIALS backend; for a list {x, y}: {x -> ..., y -> ...}.
// Samples are 12-significant-digit decimal rationals until Expr gains a Real kind.
class OdeintBackend final : public MathBackend {
public:
    std::string name() const override { return "odeint"; }
    bool supports(std::string_view head) const override { return head == "NDSolve"; }
    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;

    // Method used by the last successful solve: "DormandPrince" or "Rosenbrock".
    const std::string& last_method() const { return last_method_; }

private:
    std::string last_method_;
};

}  // namespace symats
