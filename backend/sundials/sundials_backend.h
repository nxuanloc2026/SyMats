// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include "symats/backend.h"

namespace symats {

class SundialsBackend : public MathBackend {
public:
    SundialsBackend() = default;
    ~SundialsBackend() override = default;

    ExprPtr dsolve(const ExprPtr& expr) override;
};

}  // namespace symats
