// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include "symats/expr.h"

namespace symats {

// Compute partial derivative of `f` with respect to `x`.
ExprPtr diff(const ExprPtr& f, const ExprPtr& x);

// Expand multiplication over addition and positive integer powers of sums.
ExprPtr expand(const ExprPtr& e);

}  // namespace symats
