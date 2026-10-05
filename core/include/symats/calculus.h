// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include "symats/expr.h"

namespace symats {

// Native symbolic operations shared by the evaluator and backend verification.
ExprPtr differentiate(const ExprPtr& expression, const ExprPtr& variable);
ExprPtr expand(const ExprPtr& expression);

}  // namespace symats
