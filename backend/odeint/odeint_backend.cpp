// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include "symats/eval.h"
#include "symats/pattern.h"

namespace symats {

bool OdeintBackend::supports(std::string_view head) const {
    return head == "NDSolve" || head == "Gamma" || head == "Erf" ||
           head == "Erfc" || head == "BesselJ";
}

namespace {

double eval_to_double(const ExprPtr& expr, const Context& ctx) {
    ExprPtr evaled = evaluate(expr, const_cast<Context&>(ctx));
    if (evaled->is_integer()) return evaled->integer().to_double();
    if (evaled->is_rational()) return evaled->rational().to_double();
    return 0.0;
}

ExprPtr double_to_expr(double val) {
    if (std::isnan(val) || std::isinf(val)) return make_symbol("Indeterminate");
    long long num = static_cast<long long>(std::round(val * 1000000000.0));
    long long den = 1000000000;
    if (num == 0) return make_integer(0);
    return make_rational(num, den);
}

// Simple classical 4th-order Runge-Kutta integrator for single ODE y' = f(x, y)
std::vector<std::pair<double, double>> solve_rk4(
    const std::function<double(double, double)>& f,
    double x0, double y0, double x1, std::size_t steps) {

    std::vector<std::pair<double, double>> trajectory;
    trajectory.reserve(steps + 1);
    trajectory.emplace_back(x0, y0);

    double h = (x1 - x0) / static_cast<double>(steps);
    double x = x0;
    double y = y0;

    for (std::size_t i = 0; i < steps; ++i) {
        double k1 = f(x, y);
        double k2 = f(x + 0.5 * h, y + 0.5 * h * k1);
        double k3 = f(x + 0.5 * h, y + 0.5 * h * k2);
        double k4 = f(x + h, y + h * k3);

        y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        x += h;
        trajectory.emplace_back(x, y);
    }

    return trajectory;
}

}  // namespace

std::optional<BackendResult> OdeintBackend::evaluate(const ExprPtr& expr) {
    if (!expr || !expr->head()->is_symbol()) return std::nullopt;

    const std::string_view head = expr->head()->name();

    // Special functions evaluation for numeric scalar arguments
    if (head == "Gamma" && expr->size() == 1) {
        if (expr->arg(0)->is_number()) {
            double v = expr->arg(0)->number().to_double();
            double res = std::tgamma(v);
            return BackendResult{double_to_expr(res), ResultStatus::Numeric, name()};
        }
    }
    if (head == "Erf" && expr->size() == 1) {
        if (expr->arg(0)->is_number()) {
            double v = expr->arg(0)->number().to_double();
            double res = std::erf(v);
            return BackendResult{double_to_expr(res), ResultStatus::Numeric, name()};
        }
    }
    if (head == "Erfc" && expr->size() == 1) {
        if (expr->arg(0)->is_number()) {
            double v = expr->arg(0)->number().to_double();
            double res = std::erfc(v);
            return BackendResult{double_to_expr(res), ResultStatus::Numeric, name()};
        }
    }
    if (head == "BesselJ" && expr->size() == 2) {
        if (expr->arg(0)->is_number() && expr->arg(1)->is_number()) {
            double nu = expr->arg(0)->number().to_double();
            double x = expr->arg(1)->number().to_double();
            double res = std::cyl_bessel_j(nu, x);
            return BackendResult{double_to_expr(res), ResultStatus::Numeric, name()};
        }
    }

    // NDSolve implementation: NDSolve[{y'[x] == f[x, y], y[x0] == y0}, y, {x, x0, x1}]
    if (head == "NDSolve" && expr->size() == 3) {
        const auto& eqns = expr->arg(0);
        const auto& target_var = expr->arg(1);
        const auto& domain_spec = expr->arg(2);

        if (!domain_spec || !domain_spec->has_head("List") || domain_spec->size() != 3) {
            return std::nullopt;
        }

        ExprPtr x_var = domain_spec->arg(0);
        if (!x_var->is_symbol()) return std::nullopt;
        if (!domain_spec->arg(1)->is_number() || !domain_spec->arg(2)->is_number()) {
            return std::nullopt;
        }
        double x0 = domain_spec->arg(1)->number().to_double();
        double x1 = domain_spec->arg(2)->number().to_double();

        ExprPtr ode_rhs;
        double y0 = 0.0;
        ExprPtr y_sym = target_var->is_symbol() ? target_var : nullptr;

        // Extract ODE and IC from eqns list
        if (eqns && eqns->has_head("List")) {
            for (std::size_t i = 0; i < eqns->size(); ++i) {
                const auto& item = eqns->arg(i);
                if (item->has_head("Equal") && item->size() == 2) {
                    const auto& lhs = item->arg(0);
                    const auto& rhs = item->arg(1);

                    // Check for IC: y[x0] == y0 (e.g. y[0] == 1)
                    if (lhs->is_normal() && lhs->size() == 1 && lhs->arg(0)->is_number() && rhs->is_number()) {
                        y0 = rhs->number().to_double();
                        if (!y_sym && lhs->head()->is_symbol()) y_sym = lhs->head();
                    } else {
                        // Check for ODE
                        ode_rhs = rhs;
                    }
                }
            }
        }

        if (!ode_rhs || !y_sym) return std::nullopt;

        Context eval_ctx;

        auto rhs_func = [&](double x, double y) -> double {
            // Substitute x and y values into the RHS expression
            Bindings bindings;
            bindings[x_var->name()] = double_to_expr(x);
            bindings[y_sym->name()] = double_to_expr(y);
            ExprPtr substituted = substitute(ode_rhs, bindings);
            return eval_to_double(substituted, eval_ctx);
        };

        auto trajectory = solve_rk4(rhs_func, x0, y0, x1, 100);

        ExprList interp_data;
        for (const auto& [x_val, y_val] : trajectory) {
            interp_data.push_back(make_normal("List", {double_to_expr(x_val), double_to_expr(y_val)}));
        }

        ExprPtr interp_fn = make_normal("InterpolatingFunction", {
            make_normal("List", {double_to_expr(x0), double_to_expr(x1)}),
            make_normal("List", std::move(interp_data))
        });

        ExprPtr result = make_normal("List", {
            make_normal("List", {
                make_normal("Rule", {y_sym, interp_fn})
            })
        });

        return BackendResult{result, ResultStatus::Numeric, name()};
    }

    return std::nullopt;
}

}  // namespace symats
