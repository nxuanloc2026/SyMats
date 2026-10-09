// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/plot.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "symats/eval.h"
#include "symats/numeric.h"
#include "symats/pattern.h"

namespace symats {
namespace {

struct Point {
    double x, y;
};

double quantile(std::vector<double> v, double q) {
    std::sort(v.begin(), v.end());
    const double pos = q * static_cast<double>(v.size() - 1);
    const auto i = static_cast<std::size_t>(pos);
    if (i + 1 >= v.size()) return v.back();
    return v[i] + (pos - static_cast<double>(i)) * (v[i + 1] - v[i]);
}

class Sampler {
public:
    Sampler(const std::function<double(double)>& f, const SampleOptions& o, double scale)
        : f_(f), o_(o), scale_(scale) {}

    // Emits the samples in (x0, x1], refining where the curve bends.
    void refine(Point p0, Point p1, int depth, std::vector<Point>& out) const {
        const double xm = (p0.x + p1.x) / 2;
        const Point pm{xm, f_(xm)};
        const bool finite = std::isfinite(p0.y) && std::isfinite(pm.y) && std::isfinite(p1.y);
        bool split = false;
        if (depth < o_.max_depth) {
            if (!finite) split = std::isfinite(p0.y) || std::isfinite(pm.y) || std::isfinite(p1.y);
            else split = std::abs(pm.y - (p0.y + p1.y) / 2) > o_.tolerance * scale_ ||
                         std::abs(p1.y - p0.y) > 0.1 * scale_;
        }
        if (split) {
            refine(p0, pm, depth + 1, out);
            refine(pm, p1, depth + 1, out);
        } else {
            out.push_back(pm);
            out.push_back(p1);
        }
    }

private:
    const std::function<double(double)>& f_;
    const SampleOptions& o_;
    double scale_;
};

}  // namespace

std::pair<std::vector<Polyline>, std::pair<double, double>> sample_curve(
    const std::function<double(double)>& f, double a, double b, const SampleOptions& o) {
    std::vector<Point> initial;
    const std::size_t n = std::max<std::size_t>(o.initial, 2);
    for (std::size_t i = 0; i <= n; ++i) {
        const double x = i == n ? b : a + (b - a) * static_cast<double>(i) / static_cast<double>(n);
        initial.push_back({x, f(x)});
    }
    std::vector<double> finite;
    for (const auto& p : initial)
        if (std::isfinite(p.y)) finite.push_back(p.y);
    if (finite.empty()) return {{}, {-1.0, 1.0}};

    // Robust scale: ignores a few huge values next to asymptotes.
    const double lo = quantile(finite, 0.05), hi = quantile(finite, 0.95);
    double scale = hi - lo;
    if (!(scale > 0.0)) scale = std::max(1.0, std::abs(hi));

    const Sampler sampler(f, o, scale);
    std::vector<Point> points{initial.front()};
    for (std::size_t i = 0; i + 1 < initial.size(); ++i) sampler.refine(initial[i], initial[i + 1], 0, points);

    // Split into continuous pieces: drop non-finite points, and break where neighbours
    // at the finest resolution still jump (asymptotes, step discontinuities).
    const double finest = (b - a) / static_cast<double>(n) / std::ldexp(1.0, o.max_depth) * 1.01;
    std::vector<Polyline> pieces;
    Polyline current;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        if (!std::isfinite(p.y)) {
            if (current.size() > 1) pieces.push_back(std::move(current));
            current.clear();
            continue;
        }
        if (!current.empty()) {
            const auto& q = current.back();
            if (p.x - q.first <= finest && std::abs(p.y - q.second) > 0.1 * scale) {
                if (current.size() > 1) pieces.push_back(std::move(current));
                current.clear();
            }
        }
        current.emplace_back(p.x, p.y);
    }
    if (current.size() > 1) pieces.push_back(std::move(current));

    // y range: everything, unless asymptotes make it much larger than the robust range.
    double ymin = finite.front(), ymax = finite.front();
    for (const auto& piece : pieces)
        for (const auto& [x, y] : piece) {
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
        }
    if (ymax - ymin > 10 * scale) {
        const double pad = 0.5 * scale;
        ymin = std::max(ymin, lo - pad);
        ymax = std::min(ymax, hi + pad);
    }
    if (!(ymax > ymin)) {
        ymin -= 1.0;
        ymax += 1.0;
    }
    return {std::move(pieces), {ymin, ymax}};
}

ExprPtr plot(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 2) return nullptr;
    const ExprPtr& range = expr->arg(1);  // held: the variable must stay a symbol
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol()) return nullptr;
    double a = 0.0, b = 0.0;
    try {
        a = numeric::constant(evaluate(range->arg(1), ctx));
        b = numeric::constant(evaluate(range->arg(2), ctx));
    } catch (const numeric::Unsupported&) {
        return nullptr;
    }
    if (!std::isfinite(a) || !std::isfinite(b) || !(b > a)) return nullptr;

    // Evaluate the body with x replaced by a fresh symbol, so a global value of x is
    // not used (like Block) and definitions such as f[x_] := ... still expand.
    std::string name = "$SymatsPlotVariable";
    while (ctx.value(name)) name += "$";
    const ExprPtr body =
        evaluate(substitute(expr->arg(0), {{range->arg(0)->name(), make_symbol(name)}}), ctx);
    const ExprList functions = body->has_head("List") ? body->args() : ExprList{body};

    ExprList curves;
    double ymin = 0.0, ymax = 0.0;
    bool first = true;
    for (const auto& fn : functions) {
        std::function<double(double)> f;
        try {
            auto compiled = std::make_shared<numeric::Compiled>(numeric::compile(fn, {{name, 0}}));
            f = [compiled](double x) { return compiled->eval(&x); };
        } catch (const numeric::Unsupported&) {
            // Not compilable as a whole (e.g. pattern definitions on numbers):
            // evaluate point by point.
            f = [&ctx, fn, name](double x) {
                try {
                    return numeric::constant(evaluate(substitute(fn, {{name, numeric::decimal(x)}}), ctx));
                } catch (const std::exception&) {
                    return std::nan("");
                }
            };
        }
        auto [pieces, yrange] = sample_curve(f, a, b);
        ExprList lines;
        for (const auto& piece : pieces) {
            ExprList pts;
            pts.reserve(piece.size());
            for (const auto& [x, y] : piece)
                pts.push_back(make_normal("List", {numeric::decimal(x), numeric::decimal(y)}));
            lines.push_back(make_normal("Line", {make_normal("List", std::move(pts))}));
        }
        curves.push_back(make_normal("List", std::move(lines)));
        if (pieces.empty()) continue;
        ymin = first ? yrange.first : std::min(ymin, yrange.first);
        ymax = first ? yrange.second : std::max(ymax, yrange.second);
        first = false;
    }
    if (first) {
        ymin = -1.0;
        ymax = 1.0;
    }
    const ExprPtr plot_range = make_normal("List", {
        make_normal("List", {numeric::decimal(a), numeric::decimal(b)}),
        make_normal("List", {numeric::decimal(ymin), numeric::decimal(ymax)})});
    return make_normal("Graphics", {make_normal("List", std::move(curves)),
                                    make_normal("Rule", {make_symbol("PlotRange"), plot_range})});
}

}  // namespace symats
