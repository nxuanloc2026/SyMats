// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string_view>
#include <vector>

#include "symats/session.h"

namespace symats {

// Parse one plain-text notebook cell. Newlines separate session statements;
// semicolons combine expressions on the same line and suppress a trailing result.
// Continuation inside delimiters or after an operator stays in the same statement.
std::vector<Statement> parse_cell(std::string_view text);

}  // namespace symats
