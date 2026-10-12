// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace symats {
namespace {

struct ODEProblem {
    ExprPtr rhs;
    ExprPtr independent;
    ExprPtr dependent;
    double time = 0.0;
    double state = 0.0;
};

double number(const ExprPtr& e) {
    if (!e->is_number()) throw std::invalid_argument("NDSolve requires numeric data");
    return e->number().to_double();
}

double eval_rhs(const ExprPtr& e, const ODEProblem& p) {
    if (e->is_number()) return number(e);
    if (e->is_symbol()) {
        if (e->is_symbol("Pi")) return 3.14159265358979323846;
        if (e->is_symbol("E")) return 2.71828182845904523536;
        if (e->is_symbol(p.independent->name())) return p.time;
        throw std::invalid_argument("unknown NDSolve symbol");
    }
    if (!e->is_normal() || !e->head()->is_symbol())
        throw std::invalid_argument("unsupported NDSolve expression");
    const auto& head = e->head()->name();
    if (head == p.dependent->name() && e->size() == 1 && equal(e->arg(0), p.independent))
        return p.state;
    if (head == "Plus") {
        double result = 0.0;
        for (const auto& arg : e->args()) result += eval_rhs(arg, p);
        return result;
    }
    if (head == "Times") {
        double result = 1.0;
        for (const auto& arg : e->args()) result *= eval_rhs(arg, p);
        return result;
    }
    if (head == "Power" && e->size() == 2)
        return std::pow(eval_rhs(e->arg(0), p), eval_rhs(e->arg(1), p));
    if (e->size() == 1) {
        const double value = eval_rhs(e->arg(0), p);
        if (head == "Sin") return std::sin(value);
        if (head == "Cos") return std::cos(value);
        if (head == "Tan") return std::tan(value);
        if (head == "Exp") return std::exp(value);
        if (head == "Log") return std::log(value);
        if (head == "Abs") return std::abs(value);
    }
    throw std::invalid_argument("unsupported NDSolve function");
}

ExprPtr decimal(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("NDSolve produced non-finite value");
    constexpr std::int64_t scale = 1000000000000LL;
    return make_rational(Integer(std::llround(value * scale)), Integer(scale));
}

// Adaptive RK45 (Dormand-Prince / Fehberg) ODE integration stepper
void rk45_step(double& t, double& y, double dt, const std::function<double(double, double)>& f) {
    double k1 = f(t, y);
    double k2 = f(t + dt * 0.25, y + dt * 0.25 * k1);
    double k3 = f(t + dt * 0.375, y + dt * (3.0 / 32.0 * k1 + 9.0 / 32.0 * k2));
    double k4 = f(t + dt * (12.0 / 13.0), y + dt * (1932.0 / 2197.0 * k1 - 7200.0 / 2197.0 * k2 + 7296.0 / 2197.0 * k3));
    double k5 = f(t + dt, y + dt * (439.0 / 216.0 * k1 - 8.0 * k2 + 3680.0 / 513.0 * k3 - 845.0 / 4104.0 * k4));

    y = y + dt * (25.0 / 216.0 * k1 + 1408.0 / 2565.0 * k3 + 2197.0 / 4104.0 * k4 - 1.0 / 5.0 * k5);
    t = t + dt;
}

std::optional<BackendResult> solve_ndsolve(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || expr->size() != 3 ||
        !expr->arg(0)->has_head("List") || !expr->arg(1)->is_symbol() ||
        !expr->arg(2)->has_head("List") || expr->arg(2)->size() != 3) return std::nullopt;
    ODEProblem problem;
    problem.dependent = expr->arg(1);
    problem.independent = expr->arg(2)->arg(0);
    const double start = number(expr->arg(2)->arg(1));
    const double finish = number(expr->arg(2)->arg(2));
    if (!(finish > start)) return std::nullopt;

    bool initial = false;
    for (const auto& equation : expr->arg(0)->args()) {
        if (!equation->has_head("Equal") || equation->size() != 2) return std::nullopt;
        const auto& lhs = equation->arg(0);
        bool is_derivative = false;
        if (lhs->is_normal() && lhs->size() == 1 &&
            lhs->head()->is_normal() && lhs->head()->size() == 1 &&
            lhs->head()->head()->is_normal() && lhs->head()->head()->size() == 1 &&
            lhs->head()->head()->head()->is_symbol("Derivative")) {
            auto order = lhs->head()->head()->arg(0)->integer().to_int64();
            if (order && *order == 1) {
                const auto& call = lhs->arg(0);
                const auto& dep = lhs->head()->arg(0);
                if (call->is_symbol() && equal(call, problem.independent) &&
                    dep->is_symbol() && dep->name() == problem.dependent->name()) {
                    is_derivative = true;
                }
            }
        }
        if (is_derivative) {
            problem.rhs = equation->arg(1);
        } else if (lhs->is_normal() && lhs->head()->is_symbol() &&
                   lhs->head()->name() == problem.dependent->name() && lhs->size() == 1 &&
                   number(lhs->arg(0)) == start) {
            problem.state = number(equation->arg(1));
            initial = true;
        } else {
            return std::nullopt;
        }
    }
    if (!problem.rhs || !initial) return std::nullopt;

    auto rhs_fn = [&](double t_cur, double y_cur) -> double {
        ODEProblem p = problem;
        p.time = t_cur;
        p.state = y_cur;
        return eval_rhs(p.rhs, p);
    };

    constexpr std::size_t samples = 101;
    ExprList points;
    points.reserve(samples);

    double t = start;
    double y = problem.state;
    points.push_back(make_normal("List", {decimal(t), decimal(y)}));

    for (std::size_t i = 1; i < samples; ++i) {
        const double target = start + (finish - start) * static_cast<double>(i) / (samples - 1);
        constexpr std::size_t sub_steps = 10;
        double dt = (target - t) / sub_steps;
        for (std::size_t s = 0; s < sub_steps; ++s) {
            rk45_step(t, y, dt, rhs_fn);
        }
        points.push_back(make_normal("List", {decimal(target), decimal(y)}));
    }

    return BackendResult{
        make_normal("InterpolatingFunction", {
            make_normal("List", {decimal(start), decimal(finish)}),
            make_normal("List", std::move(points))}),
        ResultStatus::Numeric, "odeint"};
}

std::optional<BackendResult> evaluate_special_functions(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || !expr->head()->is_symbol()) return std::nullopt;
    const auto& head = expr->head()->name();

    if (head == "Gamma" && expr->size() == 1 && expr->arg(0)->is_number()) {
        double val = std::tgamma(expr->arg(0)->number().to_double());
        return BackendResult{decimal(val), ResultStatus::Numeric, "odeint"};
    }
    if (head == "Erf" && expr->size() == 1 && expr->arg(0)->is_number()) {
        double val = std::erf(expr->arg(0)->number().to_double());
        return BackendResult{decimal(val), ResultStatus::Numeric, "odeint"};
    }
    if (head == "Erfc" && expr->size() == 1 && expr->arg(0)->is_number()) {
        double val = std::erfc(expr->arg(0)->number().to_double());
        return BackendResult{decimal(val), ResultStatus::Numeric, "odeint"};
    }
    if (head == "BesselJ" && expr->size() == 2 && expr->arg(0)->is_number() && expr->arg(1)->is_number()) {
        double n = expr->arg(0)->number().to_double();
        double x = expr->arg(1)->number().to_double();
        double val = std::cyl_bessel_j(n, x);
        return BackendResult{decimal(val), ResultStatus::Numeric, "odeint"};
    }

    return std::nullopt;
}

} // namespace

bool OdeintBackend::supports(std::string_view head) const {
    return head == "NDSolve" || head == "Gamma" || head == "Erf" || head == "Erfc" || head == "BesselJ";
}

std::optional<BackendResult> OdeintBackend::evaluate(const ExprPtr& expr) {
    try {
        if (expr->has_head("NDSolve")) {
            return solve_ndsolve(expr);
        }
        return evaluate_special_functions(expr);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace symats
