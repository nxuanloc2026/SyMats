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

// Two variables, on a 51 x 51 grid; values that are not real are Indeterminate.
//   Plot3D[f, {x, a, b}, {y, c, d}] -> Graphics3D[{SurfaceGrid[xs, ys, zs], ...},
//                                                PlotRange -> {{a, b}, {c, d}, {zmin, zmax}}]
//   ContourPlot[f, ...]             -> Graphics[{ContourGrid[xs, ys, zs], ...}, PlotRange -> ...]
// zs is a List of rows, one per y value (zs[[j, i]] = f[xs[[i]], ys[[j]]]).
//   ImplicitPlot[lhs == rhs, {x, a, b}, {y, c, d}] (also ContourPlot with an equation):
//   the curve by marching squares, as Graphics[{{Line[...], ...}}, ..., AspectRatio -> Automatic].
ExprPtr plot3d(const ExprPtr& expr, Context& ctx);
ExprPtr contour_plot(const ExprPtr& expr, Context& ctx);
ExprPtr implicit_plot(const ExprPtr& expr, Context& ctx);

// Zero set of g on [x0, x1] x [y0, y1] by marching squares on an n x n grid, joined
// into polylines (closed curves end where they start).
std::vector<Polyline> implicit_curve(const std::function<double(double, double)>& g, double x0, double x1,
                                     double y0, double y1, std::size_t n = 100);

}  // namespace symats
