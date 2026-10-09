// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Symats-native fallback for Integrate (AGENTS.md rule 5: backend -> native -> numeric).
// Register it after the symbolic backends. It finds antiderivatives by linearity and a
// table of elementary forms with linear arguments (x^n, 1/(a x + b), Exp, Sin, ..., c^x,
// 1/(1 + x^2), 1/Sqrt[1 - x^2]), after expanding products of sums. Every antiderivative
// is checked by differentiation before it is returned (status exact). A definite
// integral uses F[b] - F[a] when a quadrature check agrees; otherwise, with numeric
// limits, it becomes NIntegrate[...] (status numeric) for a numeric backend to evaluate.
//
// Series[f, {x, a, n}] (n <= 12): the Taylor polynomial from native derivatives, as
// SeriesData[terms, {x, a, n}]. It declines at singular points (poles, Log[0], ...), where
// a Laurent or Puiseux expansion would be needed.
//
// Solve[eqn, x] for polynomial equations of degree 1 or 2, and Solve[{eqns}, {vars}] for
// linear systems (exact elimination for numeric coefficients, Cramer's rule for up to 3
// unknowns with symbolic ones). Results are checked by substitution (exact or numeric).
//
// Limit[f, x -> a] for a finite numeric a: substitution where f is continuous, else
// L'Hopital on 0/0 quotients; kept only if f is numerically close to it on both sides.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "symats/backend.h"

namespace symats {

class NativeBackend final : public MathBackend {
public:
    std::string name() const override { return "native"; }
    bool supports(std::string_view head) const override {
        return head == "Integrate" || head == "Series" || head == "Solve" || head == "Limit";
    }
    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;
};

// Antiderivative of f with respect to the symbol x, or nullptr when no rule applies.
ExprPtr native_antiderivative(const ExprPtr& f, const ExprPtr& x);

}  // namespace symats
