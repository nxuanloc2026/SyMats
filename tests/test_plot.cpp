// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/plot.h"

#include <cmath>

#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
double num(const ExprPtr& e) { return e->number().to_double(); }

ExprPtr run(Context& ctx, const char* text) { return evaluate(parse_text(text), ctx); }

// Lines of curve k of a Graphics result.
const ExprList& lines(const ExprPtr& g, std::size_t k = 0) { return g->arg(0)->arg(k)->args(); }
}  // namespace

TEST_CASE("Plot: smooth curve is one line, adaptively refined") {
    Context ctx;
    const auto g = run(ctx, "Plot[Sin[x], {x, 0, 2*Pi}]");
    CHECK(g->has_head("Graphics"));
    CHECK(g->arg(0)->size() == 1);
    CHECK(lines(g).size() == 1);
    const auto& pts = lines(g)[0]->arg(0)->args();
    CHECK(pts.size() > 65);  // refined beyond the 64 initial intervals
    bool on_curve = true;
    for (const auto& p : pts) on_curve = on_curve && std::abs(std::sin(num(p->arg(0))) - num(p->arg(1))) < 1e-9;
    CHECK(on_curve);
    CHECK(num(pts.front()->arg(0)) == 0.0);
    CHECK(std::abs(num(pts.back()->arg(0)) - 2 * std::acos(-1.0)) < 1e-10);
    const auto& range = g->arg(1);
    CHECK(range->has_head("Rule") && range->arg(0)->is_symbol("PlotRange"));
    CHECK(std::abs(num(range->arg(1)->arg(1)->arg(0)) + 1) < 1e-3);
    CHECK(std::abs(num(range->arg(1)->arg(1)->arg(1)) - 1) < 1e-3);
}

TEST_CASE("Plot: breaks at asymptotes and outside the domain") {
    Context ctx;
    // Tan has asymptotes at Pi/2 and 3 Pi/2: three pieces, no vertical connecting lines.
    auto g = run(ctx, "Plot[Tan[x], {x, 0, 2*Pi}]");
    CHECK(lines(g).size() == 3);
    const auto& yr = g->arg(1)->arg(1)->arg(1);
    CHECK(num(yr->arg(1)) < 100);  // clipped, not the huge values next to the asymptote
    // 1/x on [-1, 1]: two pieces.
    g = run(ctx, "Plot[1/x, {x, -1, 1}]");
    CHECK(lines(g).size() == 2);
    // Sqrt[x] is not real for x < 0: one piece starting near 0.
    g = run(ctx, "Plot[Sqrt[x], {x, -1, 1}]");
    CHECK(lines(g).size() == 1);
    CHECK(num(lines(g)[0]->arg(0)->arg(0)->arg(0)) > -1e-3);
}

TEST_CASE("Plot: several functions, definitions, and a global x") {
    Context ctx;
    run(ctx, "x = 5");
    run(ctx, "f[t_] := t^2");
    const auto g = run(ctx, "Plot[{f[x], Cos[x]}, {x, 0, 1}]");
    CHECK(g->has_head("Graphics"));
    CHECK(g->arg(0)->size() == 2);
    const auto& p = lines(g, 0)[0]->arg(0)->args().back();
    CHECK(std::abs(num(p->arg(1)) - 1.0) < 1e-12);  // f[1] = 1, not f[5]
    CHECK(!run(ctx, "Plot[Sin[x], {x, 1, 0}]")->has_head("Graphics"));  // empty range stays
}

TEST_CASE("InterpolatingFunction: evaluates and plots") {
    Context ctx;
    run(ctx, "g = InterpolatingFunction[{0, 3}, {{0, 0}, {1, 1}, {2, 8}, {3, 27}}]");
    CHECK(equal(run(ctx, "g[2]"), parse_text("8")));
    CHECK(std::abs(num(run(ctx, "g[3/2]")) - 3.375) < 1e-12);  // cubic data is exact
    CHECK(run(ctx, "g[4]")->is_normal());                      // outside the domain: unevaluated
    CHECK(run(ctx, "g[y]")->is_normal());
    const auto p = run(ctx, "Plot[g[t], {t, 0, 3}]");
    CHECK(p->has_head("Graphics"));
    CHECK(std::abs(num(lines(p)[0]->arg(0)->args().back()->arg(1)) - 27.0) < 1e-9);
}

TEST_CASE("ParametricPlot and PolarPlot") {
    Context ctx;
    auto g = run(ctx, "ParametricPlot[{Cos[t], Sin[t]}, {t, 0, 2*Pi}]");
    CHECK(g->has_head("Graphics"));
    CHECK(g->size() == 3);
    CHECK(g->arg(2)->arg(0)->is_symbol("AspectRatio"));
    CHECK(lines(g).size() == 1);
    bool on_circle = true;
    for (const auto& p : lines(g)[0]->arg(0)->args())
        on_circle = on_circle && std::abs(std::hypot(num(p->arg(0)), num(p->arg(1))) - 1) < 1e-9;
    CHECK(on_circle);
    const auto& xr = g->arg(1)->arg(1)->arg(0);
    CHECK(std::abs(num(xr->arg(0)) + 1) < 1e-3 && std::abs(num(xr->arg(1)) - 1) < 1e-3);
    // Two curves.
    g = run(ctx, "ParametricPlot[{{t, t^2}, {t^2, t}}, {t, 0, 1}]");
    CHECK(g->arg(0)->size() == 2);
    // Cardioid r = 1 + Cos[th]: x reaches 2 at th = 0.
    g = run(ctx, "PolarPlot[1 + Cos[th], {th, 0, 2*Pi}]");
    CHECK(g->has_head("Graphics"));
    CHECK(std::abs(num(g->arg(1)->arg(1)->arg(0)->arg(1)) - 2) < 1e-9);
    CHECK(!run(ctx, "ParametricPlot[Sin[t], {t, 0, 1}]")->has_head("Graphics"));  // not a pair
}

TEST_CASE("Plot3D and ContourPlot sample a grid") {
    Context ctx;
    auto g = run(ctx, "Plot3D[Sin[x]*Cos[y], {x, -3, 3}, {y, 0, 2}]");
    CHECK(g->has_head("Graphics3D"));
    const auto& s = g->arg(0)->arg(0);
    CHECK(s->has_head("SurfaceGrid"));
    CHECK(s->arg(0)->size() == 51 && s->arg(1)->size() == 51 && s->arg(2)->size() == 51);
    const double x = num(s->arg(0)->arg(10)), y = num(s->arg(1)->arg(30));
    CHECK(std::abs(num(s->arg(2)->arg(30)->arg(10)) - std::sin(x) * std::cos(y)) < 1e-11);  // rows follow y
    CHECK(g->arg(1)->arg(1)->size() == 3);  // x, y and z ranges
    g = run(ctx, "Plot3D[Sqrt[x], {x, -1, 1}, {y, 0, 1}]");
    CHECK(g->arg(0)->arg(0)->arg(2)->arg(0)->arg(0)->is_symbol("Indeterminate"));
    g = run(ctx, "ContourPlot[x^2 - y^2, {x, -2, 2}, {y, -2, 2}]");
    CHECK(g->has_head("Graphics"));
    CHECK(g->arg(0)->arg(0)->has_head("ContourGrid"));
    CHECK(!run(ctx, "Plot3D[x*y, {x, 0, 1}, {x, 0, 1}]")->has_head("Graphics3D"));  // same variable
}

TEST_CASE("ImplicitPlot: marching squares joined into curves") {
    Context ctx;
    auto g = run(ctx, "ImplicitPlot[x^2 + y^2 == 1, {x, -2, 2}, {y, -2, 2}]");
    CHECK(g->has_head("Graphics"));
    CHECK(lines(g).size() == 1);
    const auto& pts = lines(g)[0]->arg(0)->args();
    CHECK(pts.size() > 100);
    CHECK(equal(pts.front(), pts.back()));  // closed
    bool on_circle = true;
    for (const auto& p : pts) on_circle = on_circle && std::abs(std::hypot(num(p->arg(0)), num(p->arg(1))) - 1) < 1e-3;
    CHECK(on_circle);
    // Two closed curves, and the two open branches of a hyperbola.
    g = run(ctx, "ImplicitPlot[(x^2 + y^2 - 1)*(x^2 + y^2 - 4) == 0, {x, -3, 3}, {y, -3, 3}]");
    CHECK(lines(g).size() == 2);
    g = run(ctx, "ContourPlot[x*y == 1, {x, -2, 2}, {y, -2, 2}]");
    CHECK(lines(g).size() == 2);
    CHECK(!equal(lines(g)[0]->arg(0)->args().front(), lines(g)[0]->arg(0)->args().back()));
    // Several equations: one curve list each.
    g = run(ctx, "ImplicitPlot[{x == y^2, y == 0}, {x, -1, 1}, {y, -1, 1}]");
    CHECK(g->arg(0)->size() == 2);
}

TEST_CASE("Animate and Slider pre-sample frames with one PlotRange") {
    Context ctx;
    auto a = run(ctx, "Animate[Plot[Sin[x - t], {x, 0, 2*Pi}], {t, 0, 10}]");
    CHECK(a->has_head("Animation"));
    CHECK(a->arg(0)->size() == 31);
    CHECK(a->arg(1)->arg(0)->is_symbol("t"));
    CHECK(a->arg(1)->arg(1)->size() == 31);
    CHECK(equal(a->arg(1)->arg(1)->arg(30), parse_text("10")));
    // Frame 0 is Sin[x]: at x = Pi/2 the curve passes through 1.
    const auto& frame = a->arg(0)->arg(0);
    CHECK(frame->has_head("Graphics"));
    // Growing amplitude: every frame gets the union range, so the axes do not jump.
    a = run(ctx, "Animate[Plot[t*Sin[x], {x, 0, 2*Pi}], {t, 1, 3, 1/2}]");
    CHECK(a->arg(0)->size() == 5);
    CHECK(equal(a->arg(0)->arg(0)->arg(1), a->arg(0)->arg(4)->arg(1)));
    CHECK(std::abs(num(a->arg(0)->arg(0)->arg(1)->arg(1)->arg(1)->arg(1)) - 3) < 1e-3);
    // Slider form, also for 3-D plots.
    a = run(ctx, "Plot[Sin[k*x], {x, 0, 2*Pi}, Slider[k, 0, 5]]");
    CHECK(a->has_head("Animation"));
    CHECK(a->size() == 3 && a->arg(2)->arg(1)->is_symbol("Slider"));
    a = run(ctx, "Plot3D[Sin[x + y + s], {x, 0, 1}, {y, 0, 1}, Slider[s, 0, 1, 1/4]]");
    CHECK(a->arg(0)->size() == 5 && a->arg(0)->arg(0)->has_head("Graphics3D"));
    CHECK(!run(ctx, "Animate[x + t, {t, 0, 1}]")->has_head("Animation"));  // frames must be plots
}

TEST_CASE("Plot sampler: direct use") {
    const auto [pieces, range] = sample_curve([](double x) { return x < 0.5 ? 0.0 : 1.0; }, 0.0, 1.0);
    CHECK(pieces.size() == 2);  // step discontinuity
    CHECK(range.first <= 0.0 && range.second >= 1.0);
    const auto none = sample_curve([](double) { return std::nan(""); }, 0.0, 1.0);
    CHECK(none.first.empty());
}
