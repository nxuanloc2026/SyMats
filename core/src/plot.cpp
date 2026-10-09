// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/plot.h"

#include <algorithm>
#include <array>
#include <map>
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

std::optional<Iterator> iterator(const ExprPtr& range, Context& ctx, const char* base = "$SymatsPlotVariable") {
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol()) return std::nullopt;
    Iterator it;
    try {
        it.a = numeric::constant(evaluate(range->arg(1), ctx));
        it.b = numeric::constant(evaluate(range->arg(2), ctx));
    } catch (const numeric::Unsupported&) {
        return std::nullopt;
    }
    if (!std::isfinite(it.a) || !std::isfinite(it.b) || !(it.b > it.a)) return std::nullopt;
    it.variable = base;
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

// ------------------------------------------------------------------ two variables

namespace {

using Fn2 = std::function<double(double, double)>;

struct Grid2 {
    Iterator x, y;
    ExprPtr body;
};

std::optional<Grid2> grid_problem(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 3) return std::nullopt;
    auto x = iterator(expr->arg(1), ctx, "$SymatsPlotX");
    auto y = iterator(expr->arg(2), ctx, "$SymatsPlotY");
    if (!x || !y || expr->arg(1)->arg(0)->name() == expr->arg(2)->arg(0)->name()) return std::nullopt;
    const ExprPtr body = evaluate(substitute(expr->arg(0), {{expr->arg(1)->arg(0)->name(), make_symbol(x->variable)},
                                                           {expr->arg(2)->arg(0)->name(), make_symbol(y->variable)}}),
                                  ctx);
    return Grid2{*x, *y, body};
}

Fn2 numeric_function2(const ExprPtr& fn, const Grid2& g, Context& ctx) {
    try {
        auto compiled = std::make_shared<numeric::Compiled>(
            numeric::compile(fn, {{g.x.variable, 0}, {g.y.variable, 1}}));
        return [compiled](double x, double y) {
            const double slots[2] = {x, y};
            return compiled->eval(slots);
        };
    } catch (const numeric::Unsupported&) {
        return [&ctx, fn, g](double x, double y) {
            try {
                return numeric::constant(evaluate(
                    substitute(fn, {{g.x.variable, numeric::decimal(x)}, {g.y.variable, numeric::decimal(y)}}), ctx));
            } catch (const std::exception&) {
                return std::nan("");
            }
        };
    }
}

std::vector<double> axis(const Iterator& it, std::size_t n) {
    std::vector<double> v(n + 1);
    for (std::size_t i = 0; i <= n; ++i) v[i] = it.a + (it.b - it.a) * static_cast<double>(i) / static_cast<double>(n);
    v[n] = it.b;
    return v;
}

// z[j][i] = f(xs[i], ys[j]) (rows follow y, as Plotly expects).
std::vector<std::vector<double>> sample_grid(const Fn2& f, const std::vector<double>& xs,
                                             const std::vector<double>& ys) {
    std::vector<std::vector<double>> z(ys.size(), std::vector<double>(xs.size()));
    for (std::size_t j = 0; j < ys.size(); ++j)
        for (std::size_t i = 0; i < xs.size(); ++i) z[j][i] = f(xs[i], ys[j]);
    return z;
}

ExprPtr numbers(const std::vector<double>& v) {
    ExprList out;
    out.reserve(v.size());
    for (double d : v) out.push_back(std::isfinite(d) ? numeric::decimal(d) : make_symbol("Indeterminate"));
    return make_normal("List", std::move(out));
}

ExprPtr grid_graphics(const ExprPtr& expr, Context& ctx, bool three_d) {
    const auto g = grid_problem(expr, ctx);
    if (!g) return nullptr;
    constexpr std::size_t n = 50;
    const auto xs = axis(g->x, n), ys = axis(g->y, n);
    ExprList items;
    std::vector<double> all;
    for (const auto& fn : g->body->has_head("List") ? g->body->args() : ExprList{g->body}) {
        const auto z = sample_grid(numeric_function2(fn, *g, ctx), xs, ys);
        ExprList rows;
        for (const auto& row : z) {
            rows.push_back(numbers(row));
            for (double v : row)
                if (std::isfinite(v)) all.push_back(v);
        }
        items.push_back(make_normal(three_d ? "SurfaceGrid" : "ContourGrid",
                                    {numbers(xs), numbers(ys), make_normal("List", std::move(rows))}));
    }
    double zmin = -1.0, zmax = 1.0;
    if (!all.empty()) {
        zmin = *std::min_element(all.begin(), all.end());
        zmax = *std::max_element(all.begin(), all.end());
        const Scale s = robust_scale(all);
        if (zmax - zmin > 10 * s.size) {
            zmin = std::max(zmin, s.lo - 0.5 * s.size);
            zmax = std::min(zmax, s.hi + 0.5 * s.size);
        }
        if (!(zmax > zmin)) {
            zmin -= 1.0;
            zmax += 1.0;
        }
    }
    ExprList range{pair(g->x.a, g->x.b), pair(g->y.a, g->y.b)};
    if (three_d) range.push_back(pair(zmin, zmax));
    return make_normal(three_d ? "Graphics3D" : "Graphics",
                       {make_normal("List", std::move(items)),
                        make_normal("Rule", {make_symbol("PlotRange"), make_normal("List", std::move(range))})});
}

}  // namespace

std::vector<Polyline> implicit_curve(const std::function<double(double, double)>& g, double x0, double x1,
                                     double y0, double y1, std::size_t n) {
    const std::vector<double> xs = axis({"", x0, x1}, n), ys = axis({"", y0, y1}, n);
    const auto z = sample_grid(g, xs, ys);
    // Grid edges carry the crossing points. Edge id: horizontal (i, j)-(i+1, j) is
    // 2 * (j * (n + 1) + i), vertical (i, j)-(i, j+1) is that + 1.
    const auto hid = [n](std::size_t i, std::size_t j) { return 2 * (j * (n + 1) + i); };
    const auto vid = [n](std::size_t i, std::size_t j) { return 2 * (j * (n + 1) + i) + 1; };
    std::map<std::size_t, std::pair<double, double>> point;
    std::map<std::size_t, std::vector<std::size_t>> links;  // edge -> segment indices
    std::vector<std::pair<std::size_t, std::size_t>> segments;
    const auto crossing = [&](std::size_t id, double xa, double ya, double za, double xb, double yb, double zb) {
        if (!point.count(id)) {
            const double t = za / (za - zb);
            point[id] = {xa + t * (xb - xa), ya + t * (yb - ya)};
        }
        return id;
    };
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i) {
            const double c[4] = {z[j][i], z[j][i + 1], z[j + 1][i + 1], z[j + 1][i]};  // counterclockwise
            if (!(std::isfinite(c[0]) && std::isfinite(c[1]) && std::isfinite(c[2]) && std::isfinite(c[3])))
                continue;
            const double px[4] = {xs[i], xs[i + 1], xs[i + 1], xs[i]}, py[4] = {ys[j], ys[j], ys[j + 1], ys[j + 1]};
            const std::size_t ids[4] = {hid(i, j), vid(i + 1, j), hid(i, j + 1), vid(i, j)};  // edge k: corner k..k+1
            std::vector<std::size_t> cut;
            for (int k = 0; k < 4; ++k) {
                const int l = (k + 1) % 4;
                if ((c[k] < 0) != (c[l] < 0)) cut.push_back(crossing(ids[k], px[k], py[k], c[k], px[l], py[l], c[l]));
            }
            if (cut.size() == 2) {
                segments.emplace_back(cut[0], cut[1]);
            } else if (cut.size() == 4) {
                // Saddle: decide the pairing from the value at the centre.
                const bool centre = (c[0] + c[1] + c[2] + c[3]) / 4 < 0;
                if (centre == (c[0] < 0)) {
                    segments.emplace_back(cut[0], cut[1]);
                    segments.emplace_back(cut[2], cut[3]);
                } else {
                    segments.emplace_back(cut[1], cut[2]);
                    segments.emplace_back(cut[3], cut[0]);
                }
            }
        }
    for (std::size_t s = 0; s < segments.size(); ++s) {
        links[segments[s].first].push_back(s);
        links[segments[s].second].push_back(s);
    }
    // Join segments into polylines by walking shared edges.
    std::vector<bool> used(segments.size(), false);
    std::vector<Polyline> lines;
    const auto walk = [&](std::size_t edge, std::size_t from, std::vector<std::size_t>& chain) {
        while (true) {
            std::size_t next = segments.size();
            for (std::size_t s : links[edge])
                if (!used[s] && s != from) next = s;
            if (next == segments.size()) return;
            used[next] = true;
            edge = segments[next].first == edge ? segments[next].second : segments[next].first;
            chain.push_back(edge);
            from = next;
        }
    };
    // Open chains start at edges with a single segment; closed loops are handled after.
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t s = 0; s < segments.size(); ++s) {
            if (used[s]) continue;
            const auto [a, b] = segments[s];
            if (pass == 0 && links[a].size() != 1 && links[b].size() != 1) continue;
            const std::size_t start = links[a].size() == 1 ? a : b, other = start == a ? b : a;
            used[s] = true;
            std::vector<std::size_t> chain{start, other};
            walk(other, s, chain);
            Polyline line;
            for (std::size_t e : chain) line.push_back(point[e]);
            lines.push_back(std::move(line));
        }
    return lines;
}

ExprPtr plot3d(const ExprPtr& expr, Context& ctx) { return grid_graphics(expr, ctx, true); }

ExprPtr implicit_plot(const ExprPtr& expr, Context& ctx) {
    const auto g = grid_problem(expr, ctx);
    if (!g) return nullptr;
    ExprList curves;
    for (const auto& eq : g->body->has_head("List") ? g->body->args() : ExprList{g->body}) {
        if (!eq->has_head("Equal") || eq->size() != 2) return nullptr;
        const Fn2 f = numeric_function2(subtract(eq->arg(0), eq->arg(1)), *g, ctx);
        CurveSamples s;
        s.pieces = implicit_curve(f, g->x.a, g->x.b, g->y.a, g->y.b);
        curves.push_back(curve(s));
    }
    Bounds bounds;
    bounds.x = {g->x.a, g->x.b};
    bounds.y = {g->y.a, g->y.b};
    return graphics(std::move(curves), bounds, true);
}

ExprPtr contour_plot(const ExprPtr& expr, Context& ctx) {
    if (expr->size() == 3 && (expr->arg(0)->has_head("Equal") ||
                              (expr->arg(0)->has_head("List") && expr->arg(0)->size() > 0 &&
                               expr->arg(0)->arg(0)->has_head("Equal"))))
        return implicit_plot(expr, ctx);
    return grid_graphics(expr, ctx, false);
}

// ------------------------------------------------------------------ animation

namespace {

// Widens `into` (a PlotRange value {{xmin, xmax}, ...}) to also cover `range`.
ExprPtr union_range(const ExprPtr& into, const ExprPtr& range) {
    if (!into) return range;
    if (into->size() != range->size()) return into;
    ExprList out;
    for (std::size_t k = 0; k < into->size(); ++k) {
        const double lo = std::min(into->arg(k)->arg(0)->number().to_double(), range->arg(k)->arg(0)->number().to_double());
        const double hi = std::max(into->arg(k)->arg(1)->number().to_double(), range->arg(k)->arg(1)->number().to_double());
        out.push_back(pair(lo, hi));
    }
    return make_normal("List", std::move(out));
}

ExprPtr plot_range(const ExprPtr& g) {
    for (const auto& a : g->args())
        if (a->has_head("Rule") && a->size() == 2 && a->arg(0)->is_symbol("PlotRange")) return a->arg(1);
    return nullptr;
}

ExprPtr with_range(const ExprPtr& g, const ExprPtr& range) {
    ExprList args;
    for (const auto& a : g->args())
        args.push_back(a->has_head("Rule") && a->arg(0)->is_symbol("PlotRange")
                           ? make_normal("Rule", {a->arg(0), range}) : a);
    return make_normal(g->head(), std::move(args));
}

}  // namespace

ExprPtr animate(const ExprPtr& plot_expr, const ExprPtr& control, Context& ctx, bool slider) {
    if (!control->has_head("List") || (control->size() != 3 && control->size() != 4) ||
        !control->arg(0)->is_symbol()) return nullptr;
    double a = 0.0, b = 0.0, step = 0.0;
    try {
        a = numeric::constant(evaluate(control->arg(1), ctx));
        b = numeric::constant(evaluate(control->arg(2), ctx));
        if (control->size() == 4) step = numeric::constant(evaluate(control->arg(3), ctx));
    } catch (const numeric::Unsupported&) {
        return nullptr;
    }
    if (!std::isfinite(a) || !std::isfinite(b) || !(b > a)) return nullptr;
    std::size_t count = 31;  // 30 steps by default
    if (control->size() == 4) {
        if (!(step > 0.0) || (b - a) / step > 1000) return nullptr;
        count = static_cast<std::size_t>(std::floor((b - a) / step + 1e-9)) + 1;
    }
    ExprList frames, values;
    ExprPtr range;
    for (std::size_t i = 0; i < count; ++i) {
        const double v = control->size() == 4 ? a + step * static_cast<double>(i)
                                              : a + (b - a) * static_cast<double>(i) / static_cast<double>(count - 1);
        const ExprPtr value = numeric::decimal(v);
        ExprPtr frame = evaluate(substitute(plot_expr, {{control->arg(0)->name(), value}}), ctx);
        if (!frame->has_head("Graphics") && !frame->has_head("Graphics3D")) return nullptr;
        if (const ExprPtr r = plot_range(frame)) range = union_range(range, r);
        frames.push_back(std::move(frame));
        values.push_back(value);
    }
    if (range)
        for (auto& f : frames) f = with_range(f, range);
    ExprList args{make_normal("List", std::move(frames)),
                  make_normal("List", {control->arg(0), make_normal("List", std::move(values))})};
    if (slider) args.push_back(make_normal("Rule", {make_symbol("Control"), make_symbol("Slider")}));
    return make_normal("Animation", std::move(args));
}

ExprPtr animate(const ExprPtr& expr, Context& ctx) {
    if (expr->size() != 2) return nullptr;
    return animate(expr->arg(0), expr->arg(1), ctx, false);
}

ExprPtr with_slider(const ExprPtr& expr, Context& ctx) {
    if (expr->size() < 2 || !expr->args().back()->has_head("Slider")) return nullptr;
    const ExprPtr& s = expr->args().back();
    if (s->size() != 3 && s->size() != 4) return nullptr;
    ExprList plot_args(expr->args().begin(), expr->args().end() - 1);
    return animate(make_normal(expr->head(), std::move(plot_args)), make_normal("List", s->args()), ctx, true);
}

}  // namespace symats
