// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <optional>

#include "symats/backend.h"

namespace symats {

// Converts through Giac without evaluating. Primarily used to check that the
// bridge preserves Expr's exact tree when a head has no Giac equivalent.
std::optional<ExprPtr> giac_roundtrip(const ExprPtr& expr);

class GiacBackend final : public MathBackend {
public:
    std::string name() const override { return "giac"; }
    bool supports(std::string_view head) const override;
    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;
};

}  // namespace symats
