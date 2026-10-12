// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "symats/expr.h"

namespace symats {

class Context;

struct IntegrationBound {
    std::string var;
    double lower = 0.0;
    double upper = 0.0;
};

struct VegasResult {
    double value = 0.0;
    double error = 0.0;
    bool status_ok = false;
};

// Vegas Monte Carlo adaptive multi-dimensional integration algorithm.
VegasResult vegas_integrate(
    const std::function<double(const std::vector<double>&)>& integrand,
    std::size_t dimensions,
    const std::vector<std::pair<double, double>>& bounds,
    std::size_t samples_per_iter = 2000,
    std::size_t max_iterations = 5);

// Evaluates NIntegrate[expr, {x, a, b}, ...]
ExprPtr evaluate_nintegrate(const ExprPtr& expr, Context& ctx);

} // namespace symats
