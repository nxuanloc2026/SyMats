// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.h"

#include <cmath>
#include <memory>

#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
// Value column of sample i of InterpolatingFunction[{a, b}, {{t, y}, ...}].
double sample(const ExprPtr& fn, std::size_t i, std::size_t column = 1) {
    return fn->arg(1)->arg(i)->arg(column)->number().to_double();
}
double last(const ExprPtr& fn) { return sample(fn, fn->arg(1)->size() - 1); }
bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }

std::optional<BackendResult> solve(OdeintBackend& backend, const char* text) {
    return backend.evaluate(parse_text(text));
}
}  // namespace

TEST_CASE("Odeint backend declines unsupported NDSolve shapes") {
    OdeintBackend backend;
    CHECK(backend.supports("NDSolve"));
    CHECK(!backend.supports("DSolve"));
    CHECK(!solve(backend, "NDSolve[{y'[x]==-y[x]},y,{x,0,1}]"));                  // no initial value
    CHECK(!solve(backend, "NDSolve[{y'[x]==-y[x],y[0]==1},y,{x,1,0}]"));          // empty range
    CHECK(!solve(backend, "NDSolve[{y'[x]==-a*y[x],y[0]==1},y,{x,0,1}]"));        // free parameter
    CHECK(!solve(backend, "NDSolve[{y'[x]==-y[x],y[1]==1},y,{x,0,1}]"));          // condition not at start
    CHECK(!solve(backend, "NDSolve[{y''[x]==-y[x],y[0]==1},y,{x,0,1}]"));         // missing y'[0]
    CHECK(!solve(backend, "NDSolve[{y'[x]==y''[x],y[0]==1},y,{x,0,1}]"));         // implicit form
    CHECK(!solve(backend, "NDSolve[{y'[x]==y[x-1],y[0]==1},y,{x,0,1}]"));         // delay
    CHECK(!solve(backend, "NDSolve[{y'[x]==-y[x],y[0]==1,y[0]==2},y,{x,0,1}]"));  // duplicate
}

TEST_CASE("Odeint backend solves scalar first-order NDSolve") {
    OdeintBackend backend;
    const auto r = solve(backend, "NDSolve[{y'[x]==-y[x],y[0]==1},y,{x,0,1}]");
    CHECK(r.has_value());
    if (!r) return;
    CHECK(r->status == ResultStatus::Numeric);
    CHECK(r->backend == "odeint");
    CHECK(backend.last_method() == "DormandPrince");
    CHECK(r->value->has_head("InterpolatingFunction"));
    CHECK(r->value->arg(1)->size() == 101);
    CHECK(near(sample(r->value, 50, 0), 0.5, 1e-12));
    CHECK(near(sample(r->value, 50), std::exp(-0.5), 1e-9));
    CHECK(near(last(r->value), std::exp(-1.0), 1e-9));
}

TEST_CASE("Odeint backend: second order, time dependence and swapped sides") {
    OdeintBackend backend;
    // y'' = -y, y(0) = 0, y'(0) = 1  ->  Sin[t] on {t, 0, Pi}.
    auto r = solve(backend, "NDSolve[{y''[t]==-y[t],y[0]==0,y'[0]==1},y,{t,0,Pi}]");
    CHECK(r.has_value());
    if (r) {
        CHECK(near(sample(r->value, 50), 1.0, 1e-8));
        CHECK(near(last(r->value), 0.0, 1e-8));
        CHECK(near(sample(r->value, 100, 0), 3.14159265359, 1e-11));
    }
    // 2*t == y'[t] with the derivative on the right: y = t^2 + 1.
    r = solve(backend, "NDSolve[{2*t==y'[t],1==y[0]},y[t],{t,0,2}]");
    CHECK(r.has_value());
    if (r) CHECK(near(last(r->value), 5.0, 1e-9));
}

TEST_CASE("Odeint backend solves systems and returns rules") {
    OdeintBackend backend;
    const auto r = solve(backend,
        "NDSolve[{x'[t]==y[t],y'[t]==-x[t],x[0]==1,y[0]==0},{x,y},{t,0,2*Pi}]");
    CHECK(r.has_value());
    if (!r) return;
    CHECK(r->value->has_head("List"));
    CHECK(r->value->size() == 2);
    CHECK(r->value->arg(0)->has_head("Rule"));
    CHECK(r->value->arg(0)->arg(0)->is_symbol("x"));
    CHECK(r->value->arg(1)->arg(0)->is_symbol("y"));
    const auto& x = r->value->arg(0)->arg(1);
    const auto& y = r->value->arg(1)->arg(1);
    CHECK(near(sample(x, 50), -1.0, 1e-8));  // Cos[Pi]
    CHECK(near(sample(y, 25), -1.0, 1e-8));  // -Sin[Pi/2]
    CHECK(near(last(x), 1.0, 1e-8));
}

TEST_CASE("Odeint backend switches to Rosenbrock for stiff problems") {
    OdeintBackend backend;
    // Robertson chemical kinetics (Hairer & Wanner, stiff test set) at t = 40.
    auto r = solve(backend,
        "NDSolve[{a'[t]==-4/100*a[t]+10000*b[t]*c[t],"
        "b'[t]==4/100*a[t]-10000*b[t]*c[t]-30000000*b[t]^2,"
        "c'[t]==30000000*b[t]^2,a[0]==1,b[0]==0,c[0]==0},{a,b,c},{t,0,40}]");
    CHECK(r.has_value());
    CHECK(backend.last_method() == "Rosenbrock");
    if (r) {
        CHECK(near(last(r->value->arg(0)->arg(1)), 0.7158270687, 1e-7));
        CHECK(near(last(r->value->arg(1)->arg(1)), 9.185534765e-6, 1e-10));
        CHECK(near(last(r->value->arg(2)->arg(1)), 0.2841637457, 1e-7));
    }
    // Van der Pol with mu = 1000 (classic stiff test); y stays in [-2.1, 2.1].
    r = solve(backend,
        "NDSolve[{y''[t]==1000*(1-y[t]^2)*y'[t]-y[t],y[0]==2,y'[0]==0},y,{t,0,3000}]");
    CHECK(r.has_value());
    CHECK(backend.last_method() == "Rosenbrock");
    if (r) {
        bool bounded = true;
        for (std::size_t i = 0; i < r->value->arg(1)->size(); ++i)
            bounded = bounded && std::abs(sample(r->value, i)) < 2.1;
        CHECK(bounded);
    }
}

TEST_CASE("Odeint backend evaluates Boost.Math special functions") {
    OdeintBackend backend;
    // y' = Erf[t], y(0) = 0  ->  y(1) = Erf[1] - (1 - E^-1)/Sqrt[Pi].
    auto r = solve(backend, "NDSolve[{y'[t]==Erf[t],y[0]==0},y,{t,0,1}]");
    CHECK(r.has_value());
    if (r) CHECK(near(last(r->value), 0.842700792949715 - (1 - std::exp(-1.0)) / std::sqrt(std::acos(-1.0)), 1e-9));
    // BesselJ[0, t]' = -BesselJ[1, t], so y(5) = BesselJ[0, 5].
    r = solve(backend, "NDSolve[{y'[t]==-BesselJ[1,t],y[0]==1},y,{t,0,5}]");
    CHECK(r.has_value());
    if (r) CHECK(near(last(r->value), -0.177596771314338, 1e-9));  // BesselJ[0, 5]
    // Gamma in the right-hand side (it cancels): y' = y, so y(2) = E.
    r = solve(backend, "NDSolve[{y'[t]==Gamma[t]*y[t]/Gamma[t],y[1]==1},y,{t,1,2}]");
    CHECK(r.has_value());
    if (r) CHECK(near(last(r->value), std::exp(1.0), 1e-8));
}

TEST_CASE("Odeint backend through the evaluator") {
    Context ctx;
    ctx.backends().add(std::make_shared<OdeintBackend>());
    const auto r = evaluate_top(parse_text("NDSolve[{y'[x] == -y[x], y[0] == 1}, y, {x, 0, 5}]"), ctx);
    CHECK(r.status == ResultStatus::Numeric);
    CHECK(r.value->has_head("InterpolatingFunction"));
    if (r.value->has_head("InterpolatingFunction")) CHECK(near(last(r.value), std::exp(-5.0), 1e-9));
}

namespace {
// u at (x, t) from a PDE result, through the evaluator.
double at(const ExprPtr& fn, const char* x, const char* t) {
    Context ctx;
    const auto v = evaluate(make_normal(fn, {parse_text(x), parse_text(t)}), ctx);
    return v->is_number() ? v->number().to_double() : std::nan("");
}
const double kPi = std::acos(-1.0);

// Arguments evaluated as the evaluator would (D[u[x, t], t] -> Derivative[{0, 1}][u][x, t]).
std::optional<BackendResult> solve_evaluated(OdeintBackend& backend, const char* text) {
    Context ctx;
    return backend.evaluate(evaluate(parse_text(text), ctx));
}
}  // namespace

TEST_CASE("Odeint backend: heat equation by the method of lines") {
    OdeintBackend backend;
    const auto r = solve_evaluated(backend,
        "NDSolve[{D[u[x, t], t] == D[u[x, t], {x, 2}], u[x, 0] == Sin[Pi*x], u[0, t] == 0, u[1, t] == 0},"
        " u, {x, 0, 1}, {t, 0, 1/2}]");
    CHECK(r.has_value());
    if (!r) return;
    CHECK(r->status == ResultStatus::Numeric);
    CHECK(r->value->has_head("InterpolatingFunction") && r->value->size() == 4);
    CHECK(backend.last_method().rfind("MethodOfLines/", 0) == 0);
    const double exact = std::exp(-kPi * kPi / 2);  // at x = 1/2, t = 1/2
    CHECK(std::abs(at(r->value, "1/2", "1/2") / exact - 1) < 5e-3);
    CHECK(std::abs(at(r->value, "1/4", "1/10") - std::exp(-kPi * kPi / 10) * std::sin(kPi / 4)) < 1e-3);
    CHECK(std::abs(at(r->value, "0", "1/3")) < 1e-9);  // boundary value
}

TEST_CASE("Odeint backend: wave equation and Neumann boundaries") {
    OdeintBackend backend;
    auto r = solve_evaluated(backend,
        "NDSolve[{D[u[x, t], {t, 2}] == D[u[x, t], {x, 2}], u[x, 0] == Sin[Pi*x],"
        " Derivative[{0, 1}][u][x, 0] == 0, u[0, t] == 0, u[1, t] == 0}, u, {x, 0, 1}, {t, 0, 1}]");
    CHECK(r.has_value());
    if (r) CHECK(std::abs(at(r->value, "1/2", "1") + 1) < 5e-3);  // Cos[Pi t] Sin[Pi x]
    // Insulated ends: u = Exp[-Pi^2 t] Cos[Pi x]; argument order u[t, x] also works.
    r = solve_evaluated(backend,
        "NDSolve[{D[u[t, x], t] == D[u[t, x], {x, 2}], u[0, x] == Cos[Pi*x],"
        " Derivative[{0, 1}][u][t, 0] == 0, Derivative[{0, 1}][u][t, 1] == 0}, u, {t, 0, 1/10}, {x, 0, 1}]");
    CHECK(r.has_value());
    if (r) CHECK(std::abs(at(r->value, "1/10", "0") - std::exp(-kPi * kPi / 10)) < 2e-3);
}

TEST_CASE("Odeint backend: PDE results evaluate and plot; bad forms decline") {
    Context ctx;
    ctx.backends().add(std::make_shared<OdeintBackend>());
    evaluate(parse_text("sol = NDSolve[{D[u[x, t], t] == D[u[x, t], {x, 2}] + u[x, t]*(1 - u[x, t]),"
                        " u[x, 0] == x*(1 - x), u[0, t] == 0, u[1, t] == 0}, u, {x, 0, 1}, {t, 0, 1}]"), ctx);
    CHECK(evaluate(parse_text("sol"), ctx)->has_head("InterpolatingFunction"));
    const auto g = evaluate(parse_text("Plot3D[sol[x, t], {x, 0, 1}, {t, 0, 1}]"), ctx);
    CHECK(g->has_head("Graphics3D"));
    CHECK(g->arg(0)->arg(0)->arg(2)->arg(10)->arg(25)->is_number());  // compiled, not Indeterminate
    const auto frames = evaluate(parse_text("Animate[Plot[sol[x, t], {x, 0, 1}], {t, 0, 1, 1/4}]"), ctx);
    CHECK(frames->has_head("Animation") && frames->arg(0)->size() == 5);
    OdeintBackend backend;
    CHECK(!solve_evaluated(backend, "NDSolve[{D[u[x, t], t] == D[u[x, t], {x, 2}], u[x, 0] == 0, u[0, t] == 0},"
                          " u, {x, 0, 1}, {t, 0, 1}]"));  // missing right boundary
    CHECK(!solve_evaluated(backend, "NDSolve[{D[u[x, t], t] == D[u[x, t], {x, 3}], u[x, 0] == 0, u[0, t] == 0,"
                          " u[1, t] == 0}, u, {x, 0, 1}, {t, 0, 1}]"));  // third order in x
}
