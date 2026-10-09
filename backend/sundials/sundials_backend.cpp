// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "sundials_backend.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <optional>

#include <cvode/cvode.h>
#include <nvector/nvector_serial.h>
#include <sunlinsol/sunlinsol_dense.h>
#include <sunmatrix/sunmatrix_dense.h>
#include <sundials/sundials_config.h>
#include <sundials/sundials_context.h>
#include <sundials/sundials_types.h>

namespace symats {
namespace {
struct Problem {
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

double numeric(const ExprPtr& e, const Problem& p) {
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
        for (const auto& arg : e->args()) result += numeric(arg, p);
        return result;
    }
    if (head == "Times") {
        double result = 1.0;
        for (const auto& arg : e->args()) result *= numeric(arg, p);
        return result;
    }
    if (head == "Power" && e->size() == 2)
        return std::pow(numeric(e->arg(0), p), numeric(e->arg(1), p));
    if (e->size() == 1) {
        const double value = numeric(e->arg(0), p);
        if (head == "Sin") return std::sin(value);
        if (head == "Cos") return std::cos(value);
        if (head == "Tan") return std::tan(value);
        if (head == "Exp") return std::exp(value);
        if (head == "Log") return std::log(value);
        if (head == "Abs") return std::abs(value);
    }
    throw std::invalid_argument("unsupported NDSolve function");
}

int rhs(sunrealtype time, N_Vector state, N_Vector derivative, void* user_data) {
    auto& problem = *static_cast<Problem*>(user_data);
    problem.time = time;
    problem.state = NV_Ith_S(state, 0);
    try {
        NV_Ith_S(derivative, 0) = numeric(problem.rhs, problem);
        return 0;
    } catch (...) {
        return -1;
    }
}

ExprPtr decimal(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("NDSolve produced non-finite value");
    constexpr std::int64_t scale = 1000000000000LL;
    return make_rational(Integer(std::llround(value * scale)), Integer(scale));
}

struct Resources {
    SUNContext context = nullptr;
    void* cvode = nullptr;
    N_Vector state = nullptr;
    SUNMatrix matrix = nullptr;
    SUNLinearSolver linear_solver = nullptr;
    ~Resources() {
        if (cvode) CVodeFree(&cvode);
        if (linear_solver) SUNLinSolFree(linear_solver);
        if (matrix) SUNMatDestroy(matrix);
        if (state) N_VDestroy(state);
        if (context) SUNContext_Free(&context);
    }
};

std::optional<BackendResult> solve(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || expr->size() != 3 ||
        !expr->arg(0)->has_head("List") || !expr->arg(1)->is_symbol() ||
        !expr->arg(2)->has_head("List") || expr->arg(2)->size() != 3) return std::nullopt;
    Problem problem;
    problem.dependent = expr->arg(1);
    problem.independent = expr->arg(2)->arg(0);
    const double start = number(expr->arg(2)->arg(1));
    const double finish = number(expr->arg(2)->arg(2));
    if (!(finish > start)) return std::nullopt;
    bool initial = false;
    for (const auto& equation : expr->arg(0)->args()) {
        if (!equation->has_head("Equal") || equation->size() != 2) return std::nullopt;
        const auto& lhs = equation->arg(0);
        // y'[x] is Derivative[1][y][x].
        if (lhs->is_normal() && lhs->size() == 1 && lhs->head()->is_normal() &&
            lhs->head()->head()->has_head("Derivative") && lhs->head()->head()->size() == 1) {
            const auto& order = lhs->head()->head()->arg(0);
            if (!order->is_integer() || order->integer().to_int64() != std::optional<long long>(1))
                return std::nullopt;
            const auto& function = lhs->head();
            if (function->size() != 1 || !function->arg(0)->is_symbol() ||
                function->arg(0)->name() != problem.dependent->name() ||
                !equal(lhs->arg(0), problem.independent)) return std::nullopt;
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
    problem.time = start;
    numeric(problem.rhs, problem);  // throws (declines) on unsupported forms before CVODE runs

    Resources resources;
#if SUNDIALS_VERSION_MAJOR >= 7
    if (SUNContext_Create(SUN_COMM_NULL, &resources.context) != 0) return std::nullopt;
#else
    if (SUNContext_Create(nullptr, &resources.context) != 0) return std::nullopt;
#endif
    resources.state = N_VNew_Serial(1, resources.context);
    resources.cvode = CVodeCreate(CV_BDF, resources.context);
    if (!resources.state || !resources.cvode) return std::nullopt;
    NV_Ith_S(resources.state, 0) = problem.state;
    if (CVodeInit(resources.cvode, rhs, start, resources.state) != CV_SUCCESS ||
        CVodeSStolerances(resources.cvode, 1e-10, 1e-12) != CV_SUCCESS ||
        CVodeSetUserData(resources.cvode, &problem) != CV_SUCCESS) return std::nullopt;
    resources.matrix = SUNDenseMatrix(1, 1, resources.context);
    resources.linear_solver = SUNLinSol_Dense(resources.state, resources.matrix, resources.context);
    if (!resources.matrix || !resources.linear_solver ||
        CVodeSetLinearSolver(resources.cvode, resources.linear_solver, resources.matrix) != CV_SUCCESS)
        return std::nullopt;

    constexpr std::size_t samples = 101;
    ExprList points;
    points.reserve(samples);
    for (std::size_t i = 0; i < samples; ++i) {
        const double target = start + (finish - start) * static_cast<double>(i) / (samples - 1);
        sunrealtype reached = start;
        if (i > 0 && CVode(resources.cvode, target, resources.state, &reached, CV_NORMAL) < 0)
            return std::nullopt;
        points.push_back(make_normal("List", {decimal(reached), decimal(NV_Ith_S(resources.state, 0))}));
    }
    return BackendResult{
        make_normal("InterpolatingFunction", {
            make_normal("List", {decimal(start), decimal(finish)}),
            make_normal("List", std::move(points))}),
        ResultStatus::Numeric, "sundials"};
}
}  // namespace

std::optional<BackendResult> SundialsBackend::evaluate(const ExprPtr& expr) {
    try { return solve(expr); }
    catch (const std::exception&) { return std::nullopt; }
}
}  // namespace symats
