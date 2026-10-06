// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "symats/backend.h"
#include "symats/expr.h"
#include "symats/pattern.h"

namespace symats {

struct RuleDefinition {
    ExprPtr lhs;
    ExprPtr rhs;
    bool is_delayed = false;
};

class Session {
public:
    Session() = default;

    // Define a rule/assignment (Set = or SetDelayed :=)
    void define_rule(const ExprPtr& lhs, const ExprPtr& rhs, bool is_delayed);

    // Evaluate an expression using current session rules and registered backend.
    ExprPtr eval(const ExprPtr& expr, std::size_t depth = 0);

    // Get current rules
    const std::vector<RuleDefinition>& rules() const { return rules_; }

private:
    std::vector<RuleDefinition> rules_;
};

// Evaluate an expression directly with a default Session instance.
ExprPtr eval(const ExprPtr& expr);

}  // namespace symats
