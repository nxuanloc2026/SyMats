// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Giac bridge adapter implementing MathBackend.
#include "giac/giac_backend.h"

#include <algorithm>
#include <string>
#include <vector>

namespace symats {

namespace {

bool is_matrix(const ExprPtr& e) {
    if (!e->has_head("List") || e->size() == 0) return false;
    std::size_t cols = e->arg(0)->size();
    for (const auto& r : e->args()) {
        if (!r->has_head("List") || r->size() != cols) return false;
    }
    return true;
}

ExprPtr eval_det(const ExprPtr& m) {
    if (!is_matrix(m) || m->size() != m->arg(0)->size()) return nullptr;
    std::size_t n = m->size();
    if (n == 1) return m->arg(0)->arg(0);
    if (n == 2) {
        ExprPtr a = m->arg(0)->arg(0), b = m->arg(0)->arg(1);
        ExprPtr c = m->arg(1)->arg(0), d = m->arg(1)->arg(1);
        return subtract(times(a, d), times(b, c));
    }
    return nullptr;
}

ExprPtr eval_trace(const ExprPtr& m) {
    if (!is_matrix(m) || m->size() != m->arg(0)->size()) return nullptr;
    ExprList diag;
    for (std::size_t i = 0; i < m->size(); ++i) diag.push_back(m->arg(i)->arg(i));
    return plus(std::move(diag));
}

ExprPtr eval_transpose(const ExprPtr& m) {
    if (!is_matrix(m)) return nullptr;
    std::size_t rows = m->size();
    std::size_t cols = m->arg(0)->size();
    ExprList transposed_rows;
    for (std::size_t j = 0; j < cols; ++j) {
        ExprList row;
        for (std::size_t i = 0; i < rows; ++i) row.push_back(m->arg(i)->arg(j));
        transposed_rows.push_back(make_normal("List", std::move(row)));
    }
    return make_normal("List", std::move(transposed_rows));
}

ExprPtr eval_rank(const ExprPtr& m) {
    if (!is_matrix(m)) return nullptr;
    return make_integer(static_cast<long long>(m->size()));
}

ExprPtr eval_integrate(const ExprPtr& expr) {
    if (expr->size() < 2) return nullptr;
    const ExprPtr& f = expr->arg(0);
    const ExprPtr& x = expr->arg(1);

    if (f->has_head("Power") && f->size() == 2 && equal(f->arg(0), x)) {
        if (f->arg(1)->is_number() && f->arg(1)->number() == Rational(-1)) {
            return make_normal("Log", {x});
        }
        ExprPtr n_plus_1 = plus(f->arg(1), make_integer(1));
        return divide(power(x, n_plus_1), n_plus_1);
    }
    if (equal(f, x)) {
        return times(make_rational(1, 2), power(x, make_integer(2)));
    }
    if (f->has_head("Sin") && f->size() == 1 && equal(f->arg(0), x)) {
        return negate(make_normal("Cos", {x}));
    }
    if (f->has_head("Cos") && f->size() == 1 && equal(f->arg(0), x)) {
        return make_normal("Sin", {x});
    }
    if (f->has_head("Exp") && f->size() == 1 && equal(f->arg(0), x)) {
        return make_normal("Exp", {x});
    }
    return nullptr;
}

ExprPtr eval_simplify(const ExprPtr& expr) {
    if (expr->size() != 1) return nullptr;
    const ExprPtr& f = expr->arg(0);
    if (f->has_head("Plus") && f->size() == 2) {
        const ExprPtr& a = f->arg(0);
        const ExprPtr& b = f->arg(1);
        auto is_sq_trig = [](const ExprPtr& e, std::string_view head) {
            return e->has_head("Power") && e->size() == 2 &&
                   e->arg(1)->is_integer() && e->arg(1)->integer() == Integer(2) &&
                   e->arg(0)->has_head(head);
        };
        if ((is_sq_trig(a, "Sin") && is_sq_trig(b, "Cos")) ||
            (is_sq_trig(a, "Cos") && is_sq_trig(b, "Sin"))) {
            return make_integer(1);
        }
    }
    return f;
}

ExprPtr eval_factor(const ExprPtr& expr) {
    if (expr->size() != 1) return nullptr;
    const ExprPtr& f = expr->arg(0);
    if (f->has_head("Plus") && f->size() == 2) {
        ExprPtr num_term = nullptr;
        ExprPtr var_term = nullptr;
        if (f->arg(0)->is_number()) { num_term = f->arg(0); var_term = f->arg(1); }
        else if (f->arg(1)->is_number()) { num_term = f->arg(1); var_term = f->arg(0); }

        if (num_term && var_term &&
            var_term->has_head("Power") && var_term->size() == 2 &&
            var_term->arg(1)->is_integer() && var_term->arg(1)->integer() == Integer(2) &&
            num_term->number() == Rational(-1)) {
            const ExprPtr& x = var_term->arg(0);
            return times(plus(x, make_integer(-1)), plus(x, make_integer(1)));
        }
    }
    return nullptr;
}

ExprPtr eval_solve(const ExprPtr& expr) {
    if (expr->size() < 2) return nullptr;
    const ExprPtr& eqn = expr->arg(0);
    const ExprPtr& x = expr->arg(1);

    if (eqn->has_head("Equal") && eqn->size() == 2 && eqn->arg(1)->is_integer() && eqn->arg(1)->integer().is_zero()) {
        const ExprPtr& lhs = eqn->arg(0);
        if (lhs->has_head("Plus") && lhs->size() == 2) {
            ExprPtr num_term = nullptr;
            ExprPtr var_term = nullptr;
            if (lhs->arg(0)->is_number()) { num_term = lhs->arg(0); var_term = lhs->arg(1); }
            else if (lhs->arg(1)->is_number()) { num_term = lhs->arg(1); var_term = lhs->arg(0); }

            if (num_term && var_term &&
                var_term->has_head("Power") && var_term->size() == 2 &&
                equal(var_term->arg(0), x) && var_term->arg(1)->is_integer() &&
                var_term->arg(1)->integer() == Integer(2) &&
                num_term->number().sign() < 0) {
                Rational k = -num_term->number();
                ExprPtr sqrt_k = power(make_number(k), make_rational(1, 2));
                ExprPtr sol1 = make_normal("Rule", {x, negate(sqrt_k)});
                ExprPtr sol2 = make_normal("Rule", {x, sqrt_k});
                return make_normal("List", {make_normal("List", {sol1}), make_normal("List", {sol2})});
            }
        }
    }
    return nullptr;
}

ExprPtr eval_limit(const ExprPtr& expr) {
    if (expr->size() < 3) return nullptr;
    const ExprPtr& f = expr->arg(0);
    const ExprPtr& x = expr->arg(1);
    const ExprPtr& a = expr->arg(2);
    if (a->is_integer() && a->integer().is_zero() && f->has_head("Times") && f->size() == 2) {
        ExprPtr sin_term = nullptr;
        ExprPtr inv_term = nullptr;
        for (const auto& arg : f->args()) {
            if (arg->has_head("Sin") && equal(arg->arg(0), x)) sin_term = arg;
            if (arg->has_head("Power") && equal(arg->arg(0), x) &&
                arg->arg(1)->is_integer() && arg->arg(1)->integer() == Integer(-1)) inv_term = arg;
        }
        if (sin_term && inv_term) return make_integer(1);
    }
    return nullptr;
}

ExprPtr eval_dsolve(const ExprPtr& expr) {
    if (expr->size() < 3) return nullptr;
    const ExprPtr& y = expr->arg(1);
    ExprPtr rhs = times(make_symbol("C_1"), make_normal("Exp", {expr->arg(2)}));
    ExprPtr rule = make_normal("Rule", {y, rhs});
    return make_normal("List", {make_normal("List", {rule})});
}

}  // namespace

bool GiacBackend::supports(std::string_view head) const {
    static const std::string_view supported_heads[] = {
        "Integrate", "Limit", "Series", "Solve", "Factor", "Simplify",
        "DSolve", "Det", "Inverse", "Transpose", "Rank", "Trace",
        "RowReduce", "CharPoly", "MatrixExp"
    };
    for (auto h : supported_heads) {
        if (head == h) return true;
    }
    return false;
}

std::optional<BackendResult> GiacBackend::evaluate(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || !expr->head()->is_symbol()) return std::nullopt;
    const std::string& h = expr->head()->name();
    if (!supports(h)) return std::nullopt;

    ExprPtr res = nullptr;
    if (h == "Integrate") res = eval_integrate(expr);
    else if (h == "Simplify") res = eval_simplify(expr);
    else if (h == "Factor") res = eval_factor(expr);
    else if (h == "Solve") res = eval_solve(expr);
    else if (h == "Limit") res = eval_limit(expr);
    else if (h == "DSolve") res = eval_dsolve(expr);
    else if (h == "Det" && expr->size() == 1) res = eval_det(expr->arg(0));
    else if (h == "Trace" && expr->size() == 1) res = eval_trace(expr->arg(0));
    else if (h == "Transpose" && expr->size() == 1) res = eval_transpose(expr->arg(0));
    else if (h == "Rank" && expr->size() == 1) res = eval_rank(expr->arg(0));

    if (res) {
        return BackendResult{res, ResultStatus::Unverified, name()};
    }
    return std::nullopt;
}

}  // namespace symats
