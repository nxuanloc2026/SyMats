// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/evaluator.h"

#include <stdexcept>

namespace symats {

void Session::define_rule(const ExprPtr& lhs, const ExprPtr& rhs, bool is_delayed) {
    if (!lhs) return;
    // Overwrite existing rule if identical lhs pattern exists
    for (auto& rule : rules_) {
        if (equal(rule.lhs, lhs)) {
            rule.rhs = rhs;
            rule.is_delayed = is_delayed;
            return;
        }
    }
    rules_.push_back({lhs, rhs, is_delayed});
}

ExprPtr Session::eval(const ExprPtr& expr, std::size_t depth) {
    if (!expr) return nullptr;
    if (depth > 256) {
        throw std::invalid_argument("eval: recursion depth limit exceeded");
    }

    // Set (a = b): evaluate rhs first, define rule, return rhs
    if (expr->has_head("Set") && expr->size() == 2) {
        ExprPtr evaluated_rhs = eval(expr->arg(1), depth + 1);
        define_rule(expr->arg(0), evaluated_rhs, /*is_delayed=*/false);
        return evaluated_rhs;
    }

    // SetDelayed (a := b): store unevaluated rhs rule, return Null
    if (expr->has_head("SetDelayed") && expr->size() == 2) {
        define_rule(expr->arg(0), expr->arg(1), /*is_delayed=*/true);
        return make_symbol("Null");
    }

    // CompoundExpression(e1, e2, ...): evaluate statements sequentially
    if (expr->has_head("CompoundExpression")) {
        ExprPtr last_res = make_symbol("Null");
        for (std::size_t i = 0; i < expr->size(); ++i) {
            last_res = eval(expr->arg(i), depth + 1);
        }
        return last_res;
    }

    // Check user-defined rules on unevaluated expression
    for (const auto& rule : rules_) {
        MatchBindings bindings;
        if (match_pattern(expr, rule.lhs, bindings)) {
            ExprPtr substituted = substitute_bindings(rule.rhs, bindings);
            return eval(substituted, depth + 1);
        }
    }

    // Normal expressions: recursively evaluate arguments
    if (expr->is_normal()) {
        ExprPtr new_head = eval(expr->head(), depth + 1);
        ExprList new_args;
        new_args.reserve(expr->size());
        for (std::size_t i = 0; i < expr->size(); ++i) {
            new_args.push_back(eval(expr->arg(i), depth + 1));
        }

        ExprPtr evaluated_normal;
        if (new_head->is_symbol("Plus")) {
            evaluated_normal = plus(std::move(new_args));
        } else if (new_head->is_symbol("Times")) {
            evaluated_normal = times(std::move(new_args));
        } else if (new_head->is_symbol("Power") && new_args.size() == 2) {
            evaluated_normal = power(new_args[0], new_args[1]);
        } else {
            evaluated_normal = make_normal(new_head, std::move(new_args));
        }

        // Re-check user-defined rules on the evaluated normal expression
        for (const auto& rule : rules_) {
            MatchBindings bindings;
            if (match_pattern(evaluated_normal, rule.lhs, bindings)) {
                ExprPtr substituted = substitute_bindings(rule.rhs, bindings);
                return eval(substituted, depth + 1);
            }
        }

        // MathBackend Dispatch (support canonical capitalized heads and lower-case text heads)
        MathBackendPtr backend = get_math_backend();
        if (backend) {
            if (evaluated_normal->has_head("Integrate") || evaluated_normal->has_head("integrate")) return backend->integrate(evaluated_normal);
            if (evaluated_normal->has_head("Solve") || evaluated_normal->has_head("solve")) return backend->solve(evaluated_normal);
            if (evaluated_normal->has_head("DSolve") || evaluated_normal->has_head("dsolve")) return backend->dsolve(evaluated_normal);
            if (evaluated_normal->has_head("Limit") || evaluated_normal->has_head("limit")) return backend->limit(evaluated_normal);
            if (evaluated_normal->has_head("Series") || evaluated_normal->has_head("series")) return backend->series(evaluated_normal);
            if (evaluated_normal->has_head("Factor") || evaluated_normal->has_head("factor")) return backend->factor(evaluated_normal);
            if (evaluated_normal->has_head("Simplify") || evaluated_normal->has_head("simplify")) return backend->simplify(evaluated_normal);
        }

        return evaluated_normal;
    }

    return expr;
}

ExprPtr eval(const ExprPtr& expr) {
    Session default_session;
    return default_session.eval(expr);
}

}  // namespace symats
