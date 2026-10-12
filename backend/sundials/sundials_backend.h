// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <optional>
#include <string_view>

#include "symats/backend.h"

namespace symats {

// CVODE-backed numeric solver for the scalar first-order subset of NDSolve.
// Samples are decimal rationals until Expr gains a Real kind.
class SundialsBackend final : public MathBackend {
public:
    std::string name() const override { return "sundials"; }
    bool supports(std::string_view head) const override { return head == "NDSolve"; }
    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;
};

}  // namespace symats
