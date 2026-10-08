// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <map>
#include <memory>
#include <set>
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
    Session();

    // Define a rule/assignment (Set = or SetDelayed :=)
    void define_rule(const ExprPtr& lhs, const ExprPtr& rhs, bool is_delayed);

    // Set attributes for a head symbol (e.g., HoldAll, Listable)
    void set_attribute(const std::string& head, const std::string& attr);
    bool has_attribute(const std::string& head, const std::string& attr) const;

    // Evaluate an expression using current session rules, attributes, and registered backend.
    ExprPtr eval(const ExprPtr& expr, std::size_t depth = 0);

    // Get current rules
    const std::vector<RuleDefinition>& rules() const { return rules_; }

private:
    std::vector<RuleDefinition> rules_;
    std::map<std::string, std::set<std::string>> attributes_;
};

// Evaluate an expression directly with a default Session instance.
ExprPtr eval(const ExprPtr& expr);

}  // namespace symats
