// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string>
#include <string_view>

#include "symats/expr.h"

namespace symats {

// Parse a serialized MathJSON expression into the shared Expr tree.
// Unsupported values and malformed JSON throw std::invalid_argument.
ExprPtr parse_mathjson(std::string_view json);

// Serialize an Expr to MathJSON. Parsing the result yields an equivalent Expr.
std::string to_mathjson(const ExprPtr& expr);

}  // namespace symats
