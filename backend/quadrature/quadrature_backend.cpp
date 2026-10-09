// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "quadrature_backend.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <boost/math/quadrature/exp_sinh.hpp>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <boost/math/quadrature/sinh_sinh.hpp>
#include <boost/math/quadrature/tanh_sinh.hpp>

#include "numeric_expr.h"
#include "vegas.h"

namespace symats {
namespace {
using numeric::Compiled;
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Decline : std::runtime_error {
    Decline() : std::runtime_error("NIntegrate form not supported") {}
};

// An integration limit: +-Infinity, or an expression of the outer variables.
struct Bound {
    double infinite = 0.0;  // -inf, +inf, or 0 when finite
    Compiled expr;
    double value(const double* slots) const { return infinite != 0.0 ? infinite : expr.eval(slots); }
};

Bound bound(const ExprPtr& e, const numeric::Slots& outer) {
    Bound b;
    if (e->is_symbol("Infinity")) b.infinite = kInf;
    else if (e->has_head("Times") && e->size() == 2 && e->arg(1)->is_symbol("Infinity") &&
             e->arg(0)->is_number() && e->arg(0)->number().to_double() < 0) b.infinite = -kInf;
    else b.expr = numeric::compile(e, outer);  // inner variables are not in `outer`
    return b;
}

struct Problem {
    Compiled integrand;
    std::vector<Bound> lower, upper;  // outermost first
    std::size_t dimension = 0;
};

Problem extract(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || expr->size() < 2) throw Decline();
    Problem p;
    numeric::Slots slots;
    for (std::size_t k = 1; k < expr->size(); ++k) {
        const auto& it = expr->arg(k);
        if (!it->has_head("List") || it->size() != 3 || !it->arg(0)->is_symbol() ||
            slots.count(it->arg(0)->name())) throw Decline();
        p.lower.push_back(bound(it->arg(1), slots));
        p.upper.push_back(bound(it->arg(2), slots));
        slots[it->arg(0)->name()] = p.dimension++;
    }
    p.integrand = numeric::compile(expr->arg(0), slots);
    return p;
}

struct Estimate {
    double value = 0.0;
    double error = 0.0;
    bool kronrod = false;  // fell back to Gauss-Kronrod
};

// One-dimensional integral: double-exponential rules first (robust at endpoint
// singularities and on infinite ranges), adaptive Gauss-Kronrod if they fail.
Estimate integrate1(const std::function<double(double)>& f, double a, double b, double tol) {
    if (a == b) return {};
    if (a > b) {
        Estimate e = integrate1(f, b, a, tol);
        e.value = -e.value;
        return e;
    }
    std::optional<Estimate> best;
    try {
        double error = 0.0, l1 = 0.0, value = 0.0;
        if (std::isfinite(a) && std::isfinite(b)) {
            static const boost::math::quadrature::tanh_sinh<double> rule;
            value = rule.integrate(f, a, b, tol, &error, &l1);
        } else if (std::isfinite(a) || std::isfinite(b)) {
            static const boost::math::quadrature::exp_sinh<double> rule;
            value = rule.integrate(f, a, b, tol, &error, &l1);
        } else {
            static const boost::math::quadrature::sinh_sinh<double> rule;
            value = rule.integrate(f, tol, &error, &l1);
        }
        if (std::isfinite(value) && std::isfinite(error)) {
            best = Estimate{value, error, false};
            if (error <= std::max(tol * l1, 1e-300) * 10) return *best;
        }
    } catch (const std::exception&) {}
    try {
        double error = 0.0;
        const double value =
            boost::math::quadrature::gauss_kronrod<double, 61>::integrate(f, a, b, 15, tol, &error);
        if (std::isfinite(value) && std::isfinite(error) && (!best || error < best->error))
            best = Estimate{value, error, true};
    } catch (const std::exception&) {}
    if (!best) throw Decline();
    return *best;
}

// Iterated integral over dimensions k.. with the outer variables already in `slots`.
Estimate nested(const Problem& p, std::size_t k, std::vector<double>& slots, double tol) {
    const double a = p.lower[k].value(slots.data());
    const double b = p.upper[k].value(slots.data());
    if (std::isnan(a) || std::isnan(b)) throw Decline();
    bool kronrod = false;
    const auto f = [&](double x) {
        slots[k] = x;
        if (k + 1 == p.dimension) return p.integrand.eval(slots.data());
        const Estimate inner = nested(p, k + 1, slots, tol);
        kronrod = kronrod || inner.kronrod;
        return inner.value;
    };
    Estimate e = integrate1(f, a, b, tol);
    e.kronrod = e.kronrod || kronrod;
    return e;
}

numeric::VegasResult monte_carlo(const Problem& p) {
    std::vector<double> slots(p.dimension);
    const auto f = [&](const double* u) {
        double jacobian = 1.0;
        for (std::size_t k = 0; k < p.dimension; ++k) {
            if (p.lower[k].infinite != 0.0 || p.upper[k].infinite != 0.0) throw Decline();
            const double a = p.lower[k].value(slots.data());
            const double b = p.upper[k].value(slots.data());
            slots[k] = a + (b - a) * u[k];
            jacobian *= b - a;
        }
        return p.integrand.eval(slots.data()) * jacobian;
    };
    return numeric::vegas(f, p.dimension);
}

}  // namespace

std::optional<BackendResult> QuadratureBackend::evaluate(const ExprPtr& expr) {
    try {
        const Problem p = extract(expr);
        double value = 0.0, error = 0.0;
        std::string method;
        if (p.dimension <= 3) {
            std::vector<double> slots(p.dimension);
            const double tol = p.dimension == 1 ? 1e-10 : p.dimension == 2 ? 1e-9 : 1e-7;
            const Estimate e = nested(p, 0, slots, tol);
            value = e.value;
            error = e.error;
            method = e.kronrod ? "GaussKronrod" : "DoubleExponential";
        } else {
            const auto r = monte_carlo(p);
            value = r.value;
            error = r.error;
            method = "Vegas";
        }
        auto result = numeric::decimal(value);
        last_error_ = error;
        last_method_ = method;
        return BackendResult{std::move(result), ResultStatus::Numeric, "quadrature"};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace symats
