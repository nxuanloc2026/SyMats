// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "symats/expr.h"
#include "symats/session.h"

namespace symats {

// Parse a plain-text expression into the shared expression tree.
// Throws std::invalid_argument with a character offset for malformed input.
ExprPtr parse_text(std::string_view input);

// Print parseable plain text. Parsing the result gives an equivalent Expr.
std::string to_text(const ExprPtr& expr);

// Parse a cell string into statements for Session::run_cell.
// Handles comments (* ... *), line continuations, ';' suppression, and '%' history expansion.
std::vector<Statement> parse_cell(std::string_view input);

}  // namespace symats
