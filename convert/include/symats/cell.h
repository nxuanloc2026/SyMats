// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "symats/expr.h"

namespace symats {

struct ParsedStatement {
    ExprPtr expr;
    bool visible = true;
    std::string raw_text;
};

// Splits a cell's plain-text input into individual statements, handling:
// - statement delimiters: newlines and semicolons ';'
// - continuation: newlines inside open brackets/braces/parentheses or after binary operators
// - output suppression: statements ending with ';' have visible = false
// - history references: %, %%, %n are transformed into Out[], Out[-2], Out[n]
std::vector<ParsedStatement> parse_cell(std::string_view cell_text);

}  // namespace symats
