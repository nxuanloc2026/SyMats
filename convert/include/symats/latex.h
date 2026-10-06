// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string>

#include "symats/expr.h"

namespace symats {

// Format an expression tree as LaTeX for math rendering.
std::string to_latex(const ExprPtr& expr);

}  // namespace symats
