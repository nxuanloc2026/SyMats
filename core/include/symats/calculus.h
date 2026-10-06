// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include "symats/expr.h"

namespace symats {

// Symbolically differentiates expr with respect to var (Symbol).
// Implements product rule, chain rule, power rule, sum rule, and elementary functions.
ExprPtr d_derivative(const ExprPtr& expr, const ExprPtr& var);

// Recursively expands polynomial products over sums and integer powers (a + b)^n.
ExprPtr expand_expr(const ExprPtr& expr);

}  // namespace symats
