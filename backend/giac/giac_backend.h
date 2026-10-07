// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Giac backend adapter implementing the MathBackend interface.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "symats/backend.h"

namespace symats {

class GiacBackend : public MathBackend {
public:
    GiacBackend() = default;

    std::string name() const override { return "giac"; }

    bool supports(std::string_view head) const override;

    std::optional<BackendResult> evaluate(const ExprPtr& expr) override;
};

}  // namespace symats
