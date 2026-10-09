// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "odeint_backend.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/zeta.hpp>
#include <boost/numeric/odeint.hpp>

#include "symats/calculus.h"

namespace symats {
namespace {
namespace odeint = boost::numeric::odeint;
using Vector = boost::numeric::ublas::vector<double>;
using Matrix = boost::numeric::ublas::matrix<double>;

struct Decline : std::runtime_error {
    Decline() : std::runtime_error("NDSolve form not supported") {}
};

// ------------------------------------------------------------ compiled numeric expressions

using Fn1 = double (*)(double);
using Fn2 = double (*)(double, double);

const std::map<std::string, Fn1, std::less<>>& unary_functions() {
    static const std::map<std::string, Fn1, std::less<>> table = {
        {"Sin", [](double x) { return std::sin(x); }},
        {"Cos", [](double x) { return std::cos(x); }},
        {"Tan", [](double x) { return std::tan(x); }},
        {"Cot", [](double x) { return 1.0 / std::tan(x); }},
        {"Sec", [](double x) { return 1.0 / std::cos(x); }},
        {"Csc", [](double x) { return 1.0 / std::sin(x); }},
        {"ArcSin", [](double x) { return std::asin(x); }},
        {"ArcCos", [](double x) { return std::acos(x); }},
        {"ArcTan", [](double x) { return std::atan(x); }},
        {"Sinh", [](double x) { return std::sinh(x); }},
        {"Cosh", [](double x) { return std::cosh(x); }},
        {"Tanh", [](double x) { return std::tanh(x); }},
        {"ArcSinh", [](double x) { return std::asinh(x); }},
        {"ArcCosh", [](double x) { return std::acosh(x); }},
        {"ArcTanh", [](double x) { return std::atanh(x); }},
        {"Exp", [](double x) { return std::exp(x); }},
        {"Log", [](double x) { return std::log(x); }},
        {"Sqrt", [](double x) { return std::sqrt(x); }},
        {"Abs", [](double x) { return std::abs(x); }},
        {"Gamma", [](double x) { return boost::math::tgamma(x); }},
        {"LogGamma", [](double x) { return boost::math::lgamma(x); }},
        {"Erf", [](double x) { return boost::math::erf(x); }},
        {"Erfc", [](double x) { return boost::math::erfc(x); }},
        {"Zeta", [](double x) { return boost::math::zeta(x); }},
    };
    return table;
}

const std::map<std::string, Fn2, std::less<>>& binary_functions() {
    static const std::map<std::string, Fn2, std::less<>> table = {
        {"Log", [](double b, double x) { return std::log(x) / std::log(b); }},
        {"ArcTan", [](double x, double y) { return std::atan2(y, x); }},
        {"Gamma", [](double a, double x) { return boost::math::tgamma(a, x); }},
        {"Beta", [](double a, double b) { return boost::math::beta(a, b); }},
        {"BesselJ", [](double n, double x) { return boost::math::cyl_bessel_j(n, x); }},
        {"BesselY", [](double n, double x) { return boost::math::cyl_neumann(n, x); }},
        {"BesselI", [](double n, double x) { return boost::math::cyl_bessel_i(n, x); }},
        {"BesselK", [](double n, double x) { return boost::math::cyl_bessel_k(n, x); }},
    };
    return table;
}

struct Node {
    enum class Op { Const, Time, State, Plus, Times, Power, Call1, Call2 } op = Op::Const;
    double value = 0.0;
    std::size_t index = 0;
    Fn1 f1 = nullptr;
    Fn2 f2 = nullptr;
    std::vector<Node> args;

    double eval(double t, const Vector& x) const {
        switch (op) {
        case Op::Const: return value;
        case Op::Time: return t;
        case Op::State: return x[index];
        case Op::Plus: {
            double r = 0.0;
            for (const auto& a : args) r += a.eval(t, x);
            return r;
        }
        case Op::Times: {
            double r = 1.0;
            for (const auto& a : args) r *= a.eval(t, x);
            return r;
        }
        case Op::Power: return std::pow(args[0].eval(t, x), args[1].eval(t, x));
        case Op::Call1: return f1(args[0].eval(t, x));
        case Op::Call2: return f2(args[0].eval(t, x), args[1].eval(t, x));
        }
        return 0.0;
    }
};

// Symbols that compile to state components, plus the independent variable.
struct Variables {
    std::string time;
    std::map<std::string, std::size_t, std::less<>> state;
};

Node compile(const ExprPtr& e, const Variables& vars) {
    Node n;
    if (e->is_number()) {
        n.value = e->number().to_double();
        return n;
    }
    if (e->is_symbol()) {
        const auto& s = e->name();
        if (s == "Pi") n.value = 3.14159265358979323846;
        else if (s == "E") n.value = 2.71828182845904523536;
        else if (s == vars.time) n.op = Node::Op::Time;
        else if (auto it = vars.state.find(s); it != vars.state.end()) {
            n.op = Node::Op::State;
            n.index = it->second;
        } else throw Decline();
        return n;
    }
    if (!e->head()->is_symbol()) throw Decline();
    const auto& head = e->head()->name();
    for (const auto& a : e->args()) n.args.push_back(compile(a, vars));
    if (head == "Plus") n.op = Node::Op::Plus;
    else if (head == "Times") n.op = Node::Op::Times;
    else if (head == "Power" && e->size() == 2) n.op = Node::Op::Power;
    else if (auto f = unary_functions().find(head); e->size() == 1 && f != unary_functions().end()) {
        n.op = Node::Op::Call1;
        n.f1 = f->second;
    } else if (auto g = binary_functions().find(head); e->size() == 2 && g != binary_functions().end()) {
        n.op = Node::Op::Call2;
        n.f2 = g->second;
    } else throw Decline();
    return n;
}

double constant(const ExprPtr& e) {
    return compile(e, Variables{}).eval(0.0, Vector());
}

// ------------------------------------------------------------ problem extraction

// A reference to an unknown: f[arg] (order 0) or Derivative[k][f][arg].
struct Ref {
    std::size_t unknown;
    long long order;
    ExprPtr arg;
};

struct Unknown {
    ExprPtr symbol;
    long long order = 0;           // order of its ODE
    std::size_t base = 0;          // first state component
    ExprPtr rhs;                   // highest derivative == rhs
    std::map<long long, double> initial;  // derivative order -> value at t0
};

std::optional<Ref> reference(const ExprPtr& e, const std::vector<Unknown>& unknowns) {
    if (!e->is_normal() || e->size() != 1) return std::nullopt;
    ExprPtr f = e->head();
    long long order = 0;
    if (f->is_normal() && f->size() == 1 && f->head()->has_head("Derivative") &&
        f->head()->size() == 1 && f->head()->arg(0)->is_integer()) {
        const auto k = f->head()->arg(0)->integer().to_int64();
        if (!k || *k < 0) return std::nullopt;
        order = *k;
        f = f->arg(0);
    }
    if (!f->is_symbol()) return std::nullopt;
    for (std::size_t i = 0; i < unknowns.size(); ++i)
        if (unknowns[i].symbol->name() == f->name()) return Ref{i, order, e->arg(0)};
    return std::nullopt;
}

std::string state_name(std::size_t i) { return "odeint`state" + std::to_string(i); }

// Replaces every f[t] / Derivative[k][f][t] (k below the ODE order) by its state symbol.
ExprPtr to_state_form(const ExprPtr& e, const std::vector<Unknown>& unknowns, const ExprPtr& t) {
    if (auto r = reference(e, unknowns)) {
        const auto& u = unknowns[r->unknown];
        if (!equal(r->arg, t) || r->order >= u.order) throw Decline();
        return make_symbol(state_name(u.base + static_cast<std::size_t>(r->order)));
    }
    if (!e->is_normal()) return e;
    ExprList args;
    for (const auto& a : e->args()) args.push_back(to_state_form(a, unknowns, t));
    return make_normal(to_state_form(e->head(), unknowns, t), std::move(args));
}

struct Problem {
    std::vector<Unknown> unknowns;
    bool single = false;  // funcs given as one symbol, not a list
    double start = 0.0, finish = 0.0;
    std::size_t dimension = 0;
    Variables vars;
    std::vector<ExprPtr> rhs;  // state form, one per unknown
};

Problem extract(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || expr->size() != 3) throw Decline();
    Problem p;
    const auto& range = expr->arg(2);
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol())
        throw Decline();
    const ExprPtr t = range->arg(0);
    p.start = constant(range->arg(1));
    p.finish = constant(range->arg(2));
    if (!std::isfinite(p.start) || !std::isfinite(p.finish) || !(p.finish > p.start))
        throw Decline();

    ExprList funcs = {expr->arg(1)};
    if (expr->arg(1)->has_head("List")) funcs = expr->arg(1)->args();
    else p.single = true;
    for (auto f : funcs) {
        if (f->is_normal() && f->size() == 1 && equal(f->arg(0), t)) f = f->head();  // y[t] -> y
        if (!f->is_symbol() || equal(f, t)) throw Decline();
        for (const auto& u : p.unknowns)
            if (u.symbol->name() == f->name()) throw Decline();
        Unknown u;
        u.symbol = f;
        p.unknowns.push_back(std::move(u));
    }
    if (p.unknowns.empty()) throw Decline();

    ExprList equations = {expr->arg(0)};
    if (expr->arg(0)->has_head("List")) equations = expr->arg(0)->args();
    std::vector<std::pair<Ref, ExprPtr>> conditions;
    for (const auto& eq : equations) {
        if (!eq->has_head("Equal") || eq->size() != 2) throw Decline();
        auto lhs = reference(eq->arg(0), p.unknowns);
        ExprPtr other = eq->arg(1);
        auto swapped = reference(eq->arg(1), p.unknowns);
        // Prefer the side that is a derivative of an unknown at the independent variable.
        const auto is_ode = [&](const std::optional<Ref>& r) {
            return r && r->order > 0 && equal(r->arg, t);
        };
        if (!is_ode(lhs) && is_ode(swapped)) {
            lhs = swapped;
            other = eq->arg(0);
        } else if (!lhs && swapped) {
            lhs = swapped;
            other = eq->arg(0);
        }
        if (!lhs) throw Decline();
        if (is_ode(lhs)) {
            auto& u = p.unknowns[lhs->unknown];
            if (u.rhs) throw Decline();
            u.order = lhs->order;
            u.rhs = other;
        } else {
            conditions.emplace_back(*lhs, other);
        }
    }
    for (const auto& [ref, value] : conditions) {
        auto& u = p.unknowns[ref.unknown];
        if (ref.order >= u.order || constant(ref.arg) != p.start ||
            u.initial.count(ref.order)) throw Decline();
        u.initial[ref.order] = constant(value);
    }

    p.vars.time = t->name();
    for (auto& u : p.unknowns) {
        if (!u.rhs || u.initial.size() != static_cast<std::size_t>(u.order)) throw Decline();
        u.base = p.dimension;
        p.dimension += static_cast<std::size_t>(u.order);
    }
    for (std::size_t i = 0; i < p.dimension; ++i) p.vars.state[state_name(i)] = i;
    for (const auto& u : p.unknowns) p.rhs.push_back(to_state_form(u.rhs, p.unknowns, t));
    return p;
}

// ------------------------------------------------------------ integration

struct NonFinite : std::runtime_error {
    NonFinite() : std::runtime_error("NDSolve produced a non-finite value") {}
};
struct BudgetExceeded : std::runtime_error {
    BudgetExceeded() : std::runtime_error("NDSolve step budget exceeded") {}
};

// Counts steps over the whole integration (odeint's max_step_checker resets per sample).
struct StepBudget {
    std::size_t* used;
    std::size_t limit;
    void operator()() {
        if (++*used > limit) throw BudgetExceeded();
    }
    void reset() {}
};

struct System {
    const Problem* problem;
    std::vector<Node> highest;  // one per unknown

    void operator()(const Vector& x, Vector& dxdt, double t) const {
        for (const auto& u : problem->unknowns) {
            const auto last = u.base + static_cast<std::size_t>(u.order) - 1;
            for (auto i = u.base; i < last; ++i) dxdt[i] = x[i + 1];
        }
        for (std::size_t k = 0; k < highest.size(); ++k) {
            const auto& u = problem->unknowns[k];
            const double v = highest[k].eval(t, x);
            if (!std::isfinite(v)) throw NonFinite();
            dxdt[u.base + static_cast<std::size_t>(u.order) - 1] = v;
        }
    }
};

// Jacobian for Rosenbrock: symbolic (native D) where it compiles, else finite differences.
struct Jacobian {
    const System* system;
    std::optional<std::vector<std::vector<Node>>> rows;  // [unknown][state component]
    std::optional<std::vector<Node>> time_rows;          // [unknown]

    void operator()(const Vector& x, Matrix& jac, double t, Vector& dfdt) const {
        const auto& p = *system->problem;
        const auto n = p.dimension;
        jac = boost::numeric::ublas::zero_matrix<double>(n, n);
        for (const auto& u : p.unknowns) {
            const auto last = u.base + static_cast<std::size_t>(u.order) - 1;
            for (auto i = u.base; i < last; ++i) jac(i, i + 1) = 1.0;
        }
        Vector f0(n), f1(n);
        if (!rows || !time_rows) (*system)(x, f0, t);
        for (std::size_t k = 0; k < p.unknowns.size(); ++k) {
            const auto row = p.unknowns[k].base + static_cast<std::size_t>(p.unknowns[k].order) - 1;
            for (std::size_t j = 0; j < n; ++j) {
                if (rows) {
                    jac(row, j) = (*rows)[k][j].eval(t, x);
                } else {
                    Vector shifted = x;
                    const double h = 1e-7 * std::max(1.0, std::abs(x[j]));
                    shifted[j] += h;
                    (*system)(shifted, f1, t);
                    jac(row, j) = (f1[row] - f0[row]) / h;
                }
            }
        }
        dfdt = boost::numeric::ublas::zero_vector<double>(n);
        if (time_rows) {
            for (std::size_t k = 0; k < p.unknowns.size(); ++k)
                dfdt[p.unknowns[k].base + static_cast<std::size_t>(p.unknowns[k].order) - 1] =
                    (*time_rows)[k].eval(t, x);
        } else {
            const double h = 1e-7 * std::max(1.0, std::abs(t));
            (*system)(x, f1, t + h);
            for (std::size_t i = 0; i < n; ++i) dfdt[i] = (f1[i] - f0[i]) / h;
        }
    }
};

Jacobian make_jacobian(const System& system) {
    const auto& p = *system.problem;
    Jacobian jac{&system, std::nullopt, std::nullopt};
    try {
        std::vector<std::vector<Node>> rows;
        for (const auto& rhs : p.rhs) {
            rows.emplace_back();
            for (std::size_t j = 0; j < p.dimension; ++j)
                rows.back().push_back(compile(differentiate(rhs, make_symbol(state_name(j))), p.vars));
        }
        jac.rows = std::move(rows);
    } catch (const std::exception&) {}
    try {
        std::vector<Node> time_rows;
        for (const auto& rhs : p.rhs)
            time_rows.push_back(compile(differentiate(rhs, make_symbol(p.vars.time)), p.vars));
        jac.time_rows = std::move(time_rows);
    } catch (const std::exception&) {}
    return jac;
}

// 12 significant digits as an exact decimal rational.
ExprPtr decimal(double value) {
    if (!std::isfinite(value)) throw NonFinite();
    if (value == 0.0) return make_integer(0);
    const int exponent = static_cast<int>(std::floor(std::log10(std::abs(value))));
    int shift = 11 - exponent;
    double scaled = value;
    for (; shift > 300; shift -= 300) scaled *= 1e300;
    const long long mantissa = std::llround(scaled * std::pow(10.0, shift));
    if (shift >= 0) return make_rational(Integer(mantissa), Integer::pow(10, static_cast<unsigned>(shift)));
    return make_integer(Integer(mantissa) * Integer::pow(10, static_cast<unsigned>(-shift)));
}

constexpr std::size_t kSamples = 101;
constexpr double kAbsTol = 1e-10, kRelTol = 1e-10;
constexpr std::size_t kExplicitBudget = 20000, kStiffBudget = 200000;

// Integrates from the initial state and records every state component at each sample time.
template <class Stepper, class Sys>
std::vector<Vector> integrate(Stepper stepper, Sys sys, Vector x, const std::vector<double>& times,
                              std::size_t budget) {
    std::vector<Vector> samples;
    std::size_t used = 0;
    odeint::integrate_times(stepper, sys, x, times.begin(), times.end(),
                            (times.back() - times.front()) / 1000.0,
                            [&](const Vector& state, double) { samples.push_back(state); },
                            StepBudget{&used, budget});
    if (samples.size() != times.size()) throw Decline();
    return samples;
}

}  // namespace

std::optional<BackendResult> OdeintBackend::evaluate(const ExprPtr& expr) {
    try {
        const Problem p = extract(expr);
        System system{&p, {}};
        for (const auto& rhs : p.rhs) system.highest.push_back(compile(rhs, p.vars));
        Vector x0(p.dimension);
        for (const auto& u : p.unknowns)
            for (const auto& [k, v] : u.initial) x0[u.base + static_cast<std::size_t>(k)] = v;
        std::vector<double> times(kSamples);
        for (std::size_t i = 0; i < kSamples; ++i)
            times[i] = p.start + (p.finish - p.start) * static_cast<double>(i) / (kSamples - 1);
        times.back() = p.finish;

        std::vector<Vector> samples;
        std::string method = "DormandPrince";
        try {
            samples = integrate(odeint::make_dense_output(kAbsTol, kRelTol,
                                                          odeint::runge_kutta_dopri5<Vector>()),
                                system, x0, times, kExplicitBudget);
        } catch (const BudgetExceeded&) {
            method = "Rosenbrock";
            const Jacobian jac = make_jacobian(system);
            samples = integrate(odeint::make_dense_output(kAbsTol, kRelTol,
                                                          odeint::rosenbrock4<double>()),
                                std::make_pair(system, jac), x0, times, kStiffBudget);
        }

        const auto domain = make_normal("List", {decimal(p.start), decimal(p.finish)});
        ExprList rules;
        for (const auto& u : p.unknowns) {
            ExprList points;
            points.reserve(kSamples);
            for (std::size_t i = 0; i < kSamples; ++i)
                points.push_back(make_normal("List", {decimal(times[i]), decimal(samples[i][u.base])}));
            auto fn = make_normal("InterpolatingFunction", {domain, make_normal("List", std::move(points))});
            if (p.single) {
                last_method_ = method;
                return BackendResult{std::move(fn), ResultStatus::Numeric, "odeint"};
            }
            rules.push_back(make_normal("Rule", {u.symbol, std::move(fn)}));
        }
        last_method_ = method;
        return BackendResult{make_normal("List", std::move(rules)), ResultStatus::Numeric, "odeint"};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace symats
