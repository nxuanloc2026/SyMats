// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/pattern.h"

namespace symats {

namespace {

bool check_type_constraint(const ExprPtr& expr, const ExprPtr& type_constraint) {
    if (!type_constraint) return true;
    if (type_constraint->is_symbol("Integer")) return expr->is_integer();
    if (type_constraint->is_symbol("Rational")) return expr->is_rational();
    if (type_constraint->is_symbol("Symbol")) return expr->is_symbol();
    if (type_constraint->is_symbol("Number")) return expr->is_number();
    if (type_constraint->is_symbol("List")) return expr->has_head("List");
    if (type_constraint->is_symbol()) return expr->has_head(type_constraint->name());
    return true;
}

}  // namespace

bool match_pattern(const ExprPtr& expr, const ExprPtr& pattern, MatchBindings& bindings) {
    if (!expr || !pattern) return false;

    // Pattern(var, Blank()) or Pattern(var, Blank(head)) or Pattern(var, BlankSequence()) e.g. x_, x_Integer, x__
    if (pattern->has_head("Pattern") && pattern->size() == 2) {
        const ExprPtr& var_sym = pattern->arg(0);
        const ExprPtr& blank_expr = pattern->arg(1);
        if (blank_expr->has_head("Blank") || blank_expr->has_head("BlankSequence")) {
            ExprPtr type_constraint = blank_expr->size() > 0 ? blank_expr->arg(0) : nullptr;
            if (!check_type_constraint(expr, type_constraint)) return false;
            if (var_sym->is_symbol()) {
                const std::string& var_name = var_sym->name();
                auto it = bindings.find(var_name);
                if (it != bindings.end()) {
                    return equal(it->second, expr);
                }
                bindings[var_name] = expr;
                return true;
            }
        }
    }

    // Anonymous Blank() or Blank(head) or BlankSequence()
    if (pattern->has_head("Blank") || pattern->has_head("BlankSequence")) {
        ExprPtr type_constraint = pattern->size() > 0 ? pattern->arg(0) : nullptr;
        return check_type_constraint(expr, type_constraint);
    }

    // Atoms
    if (pattern->is_integer() || pattern->is_rational() || pattern->is_symbol()) {
        return equal(expr, pattern);
    }

    // Normal expressions: heads and args must match
    if (pattern->is_normal() && expr->is_normal()) {
        if (!match_pattern(expr->head(), pattern->head(), bindings)) return false;

        // Sequence pattern matching support for argument lists
        std::size_t expr_i = 0;
        std::size_t pat_i = 0;

        while (pat_i < pattern->size()) {
            const ExprPtr& pat_arg = pattern->arg(pat_i);
            bool is_sequence = false;
            ExprPtr seq_var_sym = nullptr;
            ExprPtr type_constraint = nullptr;

            if (pat_arg->has_head("BlankSequence")) {
                is_sequence = true;
                if (pat_arg->size() > 0) type_constraint = pat_arg->arg(0);
            } else if (pat_arg->has_head("Pattern") && pat_arg->size() == 2 &&
                       pat_arg->arg(1)->has_head("BlankSequence")) {
                is_sequence = true;
                seq_var_sym = pat_arg->arg(0);
                if (pat_arg->arg(1)->size() > 0) type_constraint = pat_arg->arg(1)->arg(0);
            }

            if (is_sequence) {
                // BlankSequence matches one or more elements
                if (pat_i + 1 == pattern->size()) {
                    // Sequence at the end: consumes all remaining expr args
                    if (expr_i >= expr->size()) return false;
                    ExprList seq_items;
                    for (; expr_i < expr->size(); ++expr_i) {
                        if (!check_type_constraint(expr->arg(expr_i), type_constraint)) return false;
                        seq_items.push_back(expr->arg(expr_i));
                    }
                    ExprPtr matched_val = seq_items.size() == 1 ? seq_items[0] : make_normal("Sequence", std::move(seq_items));
                    if (seq_var_sym && seq_var_sym->is_symbol()) {
                        bindings[seq_var_sym->name()] = matched_val;
                    }
                    return true;
                } else {
                    // Sequence in middle: consume at least 1 element
                    if (expr_i >= expr->size()) return false;
                    if (!check_type_constraint(expr->arg(expr_i), type_constraint)) return false;
                    ExprPtr matched_val = expr->arg(expr_i);
                    if (seq_var_sym && seq_var_sym->is_symbol()) {
                        bindings[seq_var_sym->name()] = matched_val;
                    }
                    ++expr_i;
                    ++pat_i;
                    continue;
                }
            }

            if (expr_i >= expr->size()) return false;
            if (!match_pattern(expr->arg(expr_i), pat_arg, bindings)) return false;
            ++expr_i;
            ++pat_i;
        }

        return expr_i == expr->size();
    }

    return false;
}

ExprPtr substitute_bindings(const ExprPtr& expr, const MatchBindings& bindings) {
    if (!expr) return nullptr;
    if (expr->is_symbol()) {
        auto it = bindings.find(expr->name());
        if (it != bindings.end()) return it->second;
        return expr;
    }
    if (expr->is_normal()) {
        ExprPtr new_head = substitute_bindings(expr->head(), bindings);
        ExprList new_args;
        new_args.reserve(expr->size());
        for (std::size_t i = 0; i < expr->size(); ++i) {
            ExprPtr sub_arg = substitute_bindings(expr->arg(i), bindings);
            if (sub_arg->has_head("Sequence")) {
                for (std::size_t j = 0; j < sub_arg->size(); ++j) {
                    new_args.push_back(sub_arg->arg(j));
                }
            } else {
                new_args.push_back(sub_arg);
            }
        }
        return make_normal(new_head, std::move(new_args));
    }
    return expr;
}

ExprPtr replace_all(const ExprPtr& expr, const ExprPtr& rule) {
    if (!expr || !rule) return expr;
    if (!rule->has_head("Rule") && !rule->has_head("RuleDelayed")) return expr;
    if (rule->size() != 2) return expr;

    const ExprPtr& lhs = rule->arg(0);
    const ExprPtr& rhs = rule->arg(1);

    MatchBindings bindings;
    if (match_pattern(expr, lhs, bindings)) {
        return substitute_bindings(rhs, bindings);
    }

    if (expr->is_normal()) {
        ExprPtr new_head = replace_all(expr->head(), rule);
        ExprList new_args;
        new_args.reserve(expr->size());
        for (std::size_t i = 0; i < expr->size(); ++i) {
            new_args.push_back(replace_all(expr->arg(i), rule));
        }
        return make_normal(new_head, std::move(new_args));
    }

    return expr;
}

ExprPtr replace_all(const ExprPtr& expr, const std::vector<ExprPtr>& rules) {
    ExprPtr result = expr;
    for (const auto& rule : rules) {
        result = replace_all(result, rule);
    }
    return result;
}

}  // namespace symats
