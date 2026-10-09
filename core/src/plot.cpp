// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/plot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "symats/eval.h"
#include "symats/numeric.h"
#include "symats/pattern.h"

namespace symats {
namespace {

using Fn = std::function<double(double)>;

struct Sample {
    double t;
    std::array<double, 2> c;  // (x, y)
    bool finite() const { return std::isfinite(c[0]) && std::isfinite(c[1]); }
};

double quantile(std::vector<double> v, double q) {
    std::sort(v.begin(), v.end());
    const double pos = q * static_cast<double>(v.size() - 1);
    const auto i = static_cast<std::size_t>(pos);
    if (i + 1 >= v.size()) return v.back();
    return v[i] + (pos - static_cast<double>(i)) * (v[i + 1] - v[i]);
}

// Robust spread of one coordinate: ignores a few huge values next to asymptotes.
struct Scale {
    double lo = 0.0, hi = 0.0, size = 1.0;
};

Scale robust_scale(const std::vector<double>& values) {
    Scale s;
    s.lo = quantile(values, 0.05);
    s.hi = quantile(values, 0.95);
    s.size = s.hi - s.lo;
    if (!(s.size > 0.0)) s.size = std::max(1.0, std::abs(s.hi));
    return s;
}

class Sampler {
public:
    Sampler(const std::array<Fn, 2>& f, const SampleOptions& o, std::array<double, 2> scale)
        : f_(f), o_(o), scale_(scale) {}

    Sample at(double t) const { return {t, {f_[0](t), f_[1](t)}}; }

    // Emits the samples in (t0, t1], refining where the curve bends or jumps.
    void refine(const Sample& s0, const Sample& s1, int depth, std::vector<Sample>& out) const {
        const Sample sm = at((s0.t + s1.t) / 2);
        bool split = false;
        if (depth < o_.max_depth) {
            if (!(s0.finite() && sm.finite() && s1.finite())) {
                split = s0.finite() || sm.finite() || s1.finite();  // locate the domain edge
            } else {
                for (std::size_t k = 0; k < 2; ++k)
                    split = split ||
                            std::abs(sm.c[k] - (s0.c[k] + s1.c[k]) / 2) > o_.tolerance * scale_[k] ||
                            std::abs(s1.c[k] - s0.c[k]) > 0.1 * scale_[k];
            }
        }
        if (split) {
            refine(s0, sm, depth + 1, out);
            refine(sm, s1, depth + 1, out);
        } else {
            out.push_back(sm);
            out.push_back(s1);
        }
    }

private:
    const std::array<Fn, 2>& f_;
    const SampleOptions& o_;
    std::array<double, 2> scale_;
};

}  // namespace

CurveSamples sample_parametric(const Fn& fx, const Fn& fy, double a, double b, const SampleOptions& o) {
    const std::array<Fn, 2> f{fx, fy};
    const std::size_t n = std::max<std::size_t>(o.initial, 2);
    std::vector<Sample> initial;
    std::array<std::vector<double>, 2> finite;
    for (std::size_t i = 0; i <= n; ++i) {
        const double t = i == n ? b : a + (b - a) * static_cast<double>(i) / static_cast<double>(n);
        initial.push_back({t, {fx(t), fy(t)}});
        if (initial.back().finite())
            for (std::size_t k = 0; k < 2; ++k) finite[k].push_back(initial.back().c[k]);
    }
    CurveSamples result;
    if (finite[0].empty()) {
        result.x_range = {a, b};
        result.y_range = {-1.0, 1.0};
        return result;
    }
    const std::array<Scale, 2> scales{robust_scale(finite[0]), robust_scale(finite[1])};

    const Sampler sampler(f, o, {scales[0].size, scales[1].size});
    std::vector<Sample> samples{initial.front()};
    for (std::size_t i = 0; i + 1 < initial.size(); ++i) sampler.refine(initial[i], initial[i + 1], 0, samples);

    // Split into continuous pieces: drop non-finite samples, and break where neighbours
    // at the finest resolution still jump (asymptotes, step discontinuities).
    const double finest = (b - a) / static_cast<double>(n) / std::ldexp(1.0, o.max_depth) * 1.01;
    Polyline current;
    double last_t = a;
    std::array<double, 2> min{finite[0][0], finite[1][0]}, max = min;
    for (const auto& s : samples) {
        if (!s.finite()) {
            if (current.size() > 1) result.pieces.push_back(std::move(current));
            current.clear();
            continue;
        }
        if (!current.empty() && s.t - last_t <= finest) {
            const auto& q = current.back();
            if (std::abs(s.c[0] - q.first) > 0.1 * scales[0].size ||
                std::abs(s.c[1] - q.second) > 0.1 * scales[1].size) {
                if (current.size() > 1) result.pieces.push_back(std::move(current));
                current.clear();
            }
        }
        current.emplace_back(s.c[0], s.c[1]);
        last_t = s.t;
        for (std::size_t k = 0; k < 2; ++k) {
            min[k] = std::min(min[k], s.c[k]);
            max[k] = std::max(max[k], s.c[k]);
        }
    }
    if (current.size() > 1) result.pieces.push_back(std::move(current));

    // Ranges: everything, unless asymptotes make them much larger than the robust range.
    std::array<std::pair<double, double>, 2> ranges;
    for (std::size_t k = 0; k < 2; ++k) {
        double lo = min[k], hi = max[k];
        if (hi - lo > 10 * scales[k].size) {
            lo = std::max(lo, scales[k].lo - 0.5 * scales[k].size);
            hi = std::min(hi, scales[k].hi + 0.5 * scales[k].size);
        }
        if (!(hi > lo)) {
            lo -= 1.0;
            hi += 1.0;
        }
        ranges[k] = {lo, hi};
    }
    result.x_range = ranges[0];
    result.y_range = ranges[1];
    return result;
}

std::pair<std::vector<Polyline>, std::pair<double, double>> sample_curve(
    const Fn& f, double a, double b, const SampleOptions& options) {
    auto r = sample_parametric([](double t) { return t; }, f, a, b, options);
    return {std::move(r.pieces), r.y_range};
}

namespace {

// A plot iterator {x, a, b}: held, so the variable stays a symbol; limits evaluated.
struct Iterator {
    std::string variable;  // fresh symbol standing for the plot variable
    double a = 0.0, b = 0.0;
};

std::optional<Iterator> iterator(const ExprPtr& range, Context& ctx) {
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol()) return std::nullopt;
    Iterator it;
    try {
        it.a = numeric::constant(evaluate(range->arg(1), ctx));
        it.b = numeric::constant(evaluate(range->arg(2), ctx));
    } catch (const numeric::Unsupported&) {
        return std::nullopt;
    }
    if (!std::isfinite(it.a) || !std::isfinite(it.b) || !(it.b > it.a)) return std::nullopt;
    it.variable = "$SymatsPlotVariable";
    while (ctx.value(it.variable)) it.variable += "$";
    return it;
}

// Evaluates the body with the plot variable replaced by the fresh symbol, so a global
// value of x is not used (like Block) and definitions such as f[x_] := ... still expand.
ExprPtr local_body(const ExprPtr& body, const ExprPtr& range, const Iterator& it, Context& ctx) {
    return evaluate(substitute(body, {{range->arg(0)->name(), make_symbol(it.variable)}}), ctx);
}

Fn numeric_function(const ExprPtr& fn, const std::string& variable, Context& ctx) {
    try {
        auto compiled = std::make_shared<numeric::Compiled>(numeric::compile(fn, {{variable, 0}}));
        return [compiled](double x) { return compiled->eval(&x); };
    } catch (const numeric::Unsupported&) {
        // Not compilable as a whole (e.g. pattern definitions on numbers): evaluate
        // point by point.
        return [&ctx, fn, variable](double x) {
            try {
                return numeric::constant(evaluate(substitute(fn, {{variable, numeric::decimal(x)}}), ctx));
            } catch (const std::exception&) {
                return std::nan("");
            }
        };
    }
}

ExprPtr pair(double a, double b) { return make_normal("List", {numeric::decimal(a), numeric::decimal(b)}); }

struct Bounds {
    bool any = false;
    std::pair<double, double> x{0.0, 0.0}, y{-1.0, 1.0};
    void add(const CurveSamples& s) {
        if (s.pieces.empty()) return;
        x = any ? std::pair{std::min(x.first, s.x_range.first), std::max(x.second, s.x_range.second)} : s.x_range;
        y = any ? std::pair{std::min(y.first, s.y_range.first), std::max(y.second, s.y_range.second)} : s.y_range;
        any = true;
    }
};

ExprPtr curve(const CurveSamples& s) {
    ExprList lines;
    for (const auto& piece : s.pieces) {
        ExprList pts;
        pts.reserve(piece.size());
        for (const auto& [x, y] : piece) pts.push_back(pair(x, y));
        lines.push_back(make_normal("Line", {make_normal("List", std::move(pts))}));
    }
    return make_normal("List", std::move(lines));
}

ExprPtr graphics(ExprList curves, const Bounds& bounds, bool equal_aspect) {
    ExprList args{make_normal("List", std::move(curves)),
                  make_normal("Rule", {make_symbol("PlotRange"),
                                       make_normal("List", {pair(bounds.x.first, bounds.x.second),
                                                            pair(bounds.y.first, bounds.y.second)})})};
    if (equal_aspect)
        args.push_back(make_normal("Rule", {make_symbol("AspectRatio"), make_symbol("Automatic")}));
    return make_normal("Graphics", std::move(args));
}

}  // namespace

ExprPtr plot(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 2) return nullptr;
    const auto it = iterator(expr->arg(1), ctx);
    if (!it) return nullptr;
    const ExprPtr body = local_body(expr->arg(0), expr->arg(1), *it, ctx);
    ExprList curves;
    Bounds bounds;
    for (const auto& fn : body->has_head("List") ? body->args() : ExprList{body}) {
        auto s = sample_parametric([](double t) { return t; }, numeric_function(fn, it->variable, ctx),
                                   it->a, it->b);
        bounds.add(s);
        curves.push_back(curve(s));
    }
    bounds.x = {it->a, it->b};
    return graphics(std::move(curves), bounds, false);
}

ExprPtr parametric_plot(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 2) return nullptr;
    const auto it = iterator(expr->arg(1), ctx);
    if (!it) return nullptr;
    const ExprPtr body = local_body(expr->arg(0), expr->arg(1), *it, ctx);
    if (!body->has_head("List") || body->size() == 0) return nullptr;
    const bool several = body->arg(0)->has_head("List");
    ExprList curves;
    Bounds bounds;
    for (const auto& xy : several ? body->args() : ExprList{body}) {
        if (!xy->has_head("List") || xy->size() != 2) return nullptr;
        const auto s = sample_parametric(numeric_function(xy->arg(0), it->variable, ctx),
                                         numeric_function(xy->arg(1), it->variable, ctx), it->a, it->b);
        bounds.add(s);
        curves.push_back(curve(s));
    }
    return graphics(std::move(curves), bounds, true);
}

ExprPtr polar_plot(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 2) return nullptr;
    const auto it = iterator(expr->arg(1), ctx);
    if (!it) return nullptr;
    const ExprPtr body = local_body(expr->arg(0), expr->arg(1), *it, ctx);
    ExprList curves;
    Bounds bounds;
    for (const auto& fn : body->has_head("List") ? body->args() : ExprList{body}) {
        const Fn r = numeric_function(fn, it->variable, ctx);
        const auto s = sample_parametric([r](double th) { return r(th) * std::cos(th); },
                                         [r](double th) { return r(th) * std::sin(th); }, it->a, it->b);
        bounds.add(s);
        curves.push_back(curve(s));
    }
    return graphics(std::move(curves), bounds, true);
}

}  // namespace symats
