// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Plot sampling. Plot[f, {x, a, b}] and Plot[{f, g, ...}, {x, a, b}] evaluate to
//   Graphics[{{Line[{{x, y}, ...}], ...}, ...}, PlotRange -> {{a, b}, {ymin, ymax}}]
// with one inner List per function and one Line per continuous piece (the curve is
// broken at singularities, jumps and points where f is not real). Coordinates are
// 12-significant-digit decimal rationals. The front end renders this with Plotly.
// ParametricPlot[{x[t], y[t]}, {t, a, b}] (or a list of pairs) and PolarPlot[r[th],
// {th, a, b}] give the same shape plus AspectRatio -> Automatic (equal axis scales).
#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "symats/expr.h"

namespace symats {

class Context;

struct SampleOptions {
    std::size_t initial = 64;   // uniform intervals before refinement
    int max_depth = 10;         // bisections per initial interval
    double tolerance = 1e-3;    // allowed deviation from a straight line, as a fraction of the y range
};

using Polyline = std::vector<std::pair<double, double>>;

// Adaptive samples of y = f(x) on [a, b] as continuous pieces. f may return NaN or inf
// where it is undefined. The second result is the suggested y range (outliers near
// asymptotes clipped).
std::pair<std::vector<Polyline>, std::pair<double, double>> sample_curve(
    const std::function<double(double)>& f, double a, double b, const SampleOptions& options = {});

struct CurveSamples {
    std::vector<Polyline> pieces;
    std::pair<double, double> x_range, y_range;  // outliers near asymptotes clipped
};

// Adaptive samples of the parametric curve (fx(t), fy(t)) for t in [a, b].
CurveSamples sample_parametric(const std::function<double(double)>& fx,
                               const std::function<double(double)>& fy, double a, double b,
                               const SampleOptions& options = {});

// The built-ins (HoldAll). They return nullptr when the arguments are not a valid plot.
ExprPtr plot(const ExprPtr& expr, Context& ctx);
ExprPtr parametric_plot(const ExprPtr& expr, Context& ctx);
ExprPtr polar_plot(const ExprPtr& expr, Context& ctx);

}  // namespace symats
