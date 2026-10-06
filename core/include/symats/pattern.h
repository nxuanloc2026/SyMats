// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <map>
#include <string>
#include <vector>
#include "symats/expr.h"

namespace symats {

using MatchBindings = std::map<std::string, ExprPtr>;

// Match an expression against a pattern. Returns true if match succeeds and populates bindings.
bool match_pattern(const ExprPtr& expr, const ExprPtr& pattern, MatchBindings& bindings);

// Substitute pattern bindings into a template expression.
ExprPtr substitute_bindings(const ExprPtr& expr, const MatchBindings& bindings);

// Replace all occurrences in expr matching rules (Rule or RuleDelayed).
ExprPtr replace_all(const ExprPtr& expr, const ExprPtr& rule);
ExprPtr replace_all(const ExprPtr& expr, const std::vector<ExprPtr>& rules);

}  // namespace symats
