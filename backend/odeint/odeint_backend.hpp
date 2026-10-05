// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "symats/backend.h"
#include "symats/expr.h"

namespace symats {

class OdeintBackend : public MathBackend {
public:
    OdeintBackend() = default;
    ~OdeintBackend() override = default;

    std::string name() const override { return "odeint"; }

    bool supports(std::string_view head) const override;

    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;
};

}  // namespace symats
