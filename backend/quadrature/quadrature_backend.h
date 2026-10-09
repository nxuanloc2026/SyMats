// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "symats/backend.h"

namespace symats {

// Numeric integration:
//   NIntegrate[f, {x, a, b}]                    a, b may be -Infinity / Infinity
//   NIntegrate[f, {x, a, b}, {y, c, d}, ...]    inner limits may depend on outer variables
// One to three dimensions use nested Boost.Math quadrature (tanh-sinh / exp-sinh /
// sinh-sinh, with adaptive Gauss-Kronrod as fallback). Four or more dimensions use
// VEGAS adaptive Monte Carlo (finite limits only), with a fixed seed so results repeat.
// The value is a 12-significant-digit decimal rational with status Numeric.
class QuadratureBackend final : public MathBackend {
public:
    std::string name() const override { return "quadrature"; }
    bool supports(std::string_view head) const override { return head == "NIntegrate"; }
    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;

    // Error estimate of the last successful integral (absolute), and the method used:
    // "DoubleExponential", "GaussKronrod" or "Vegas".
    double last_error() const { return last_error_; }
    const std::string& last_method() const { return last_method_; }

private:
    double last_error_ = 0.0;
    std::string last_method_;
};

}  // namespace symats
