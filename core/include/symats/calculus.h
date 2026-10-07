// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Native symbolic differentiation (D) and expansion (Expand).
#pragma once

#include "symats/expr.h"

namespace symats {

// Differentiate an expression with respect to var (a Symbol ExprPtr).
ExprPtr differentiate(const ExprPtr& expr, const ExprPtr& var);

// Differentiate an expression n times with respect to var.
ExprPtr differentiate(const ExprPtr& expr, const ExprPtr& var, unsigned long long n);

// Expand products of sums and integer powers of sums.
ExprPtr expand(const ExprPtr& expr);

}  // namespace symats
