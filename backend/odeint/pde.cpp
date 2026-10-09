// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// NDSolve for PDEs in one space variable by the method of lines: central differences on
// a uniform grid turn the PDE into an ODE system in time, integrated by integrator.h.
#include "pde.h"

#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "integrator.h"
#include "numeric_expr.h"
#include "symats/calculus.h"

namespace symats {
namespace {
using namespace odeint_detail;
using numeric::Compiled;

struct Decline : std::runtime_error {
    Decline() : std::runtime_error("NDSolve PDE form not supported") {}
};

constexpr std::size_t kIntervals = 50;  // space grid: 51 nodes
constexpr std::size_t kTimes = 51;      // time samples

// Slots of the compiled right-hand side.
enum Slot : std::size_t { X, T, U, UX, UXX, UT, kSlots };

Compiled compile(const ExprPtr& e, const numeric::Slots& slots) {
    try {
        return numeric::compile(e, slots, numeric::special_functions());
    } catch (const numeric::Unsupported&) {
        throw Decline();
    }
}

double constant(const ExprPtr& e) { return compile(e, {}).eval(nullptr); }

// u[a, b] or Derivative[{i, j}][u][a, b], with orders and arguments reordered to (space, time).
struct Ref {
    long long space_order = 0, time_order = 0;
    ExprPtr space_arg, time_arg;
};

struct Layout {
    std::string u;
    std::size_t space_index = 0;  // position of the space variable in u's arguments
};

std::optional<Ref> reference(const ExprPtr& e, const Layout& l) {
    if (!e->is_normal() || e->size() != 2) return std::nullopt;
    ExprPtr f = e->head();
    long long orders[2] = {0, 0};
    if (f->is_normal() && f->size() == 1 && f->head()->has_head("Derivative") && f->head()->size() == 1) {
        const ExprPtr& o = f->head()->arg(0);
        if (!o->has_head("List") || o->size() != 2) return std::nullopt;
        for (std::size_t k = 0; k < 2; ++k) {
            if (!o->arg(k)->is_integer()) return std::nullopt;
            const auto v = o->arg(k)->integer().to_int64();
            if (!v || *v < 0) return std::nullopt;
            orders[k] = *v;
        }
        f = f->arg(0);
    }
    if (!f->is_symbol(l.u)) return std::nullopt;
    const std::size_t s = l.space_index, t = 1 - s;
    return Ref{orders[s], orders[t], e->arg(s), e->arg(t)};
}

// Replaces references to u at (x, t) by slot symbols; anything else referring to u declines.
ExprPtr slot_form(const ExprPtr& e, const Layout& l, const ExprPtr& x, const ExprPtr& t, int time_order) {
    if (auto r = reference(e, l)) {
        if (!equal(r->space_arg, x) || !equal(r->time_arg, t)) throw Decline();
        if (r->time_order == 0 && r->space_order == 0) return make_symbol("pde`U");
        if (r->time_order == 0 && r->space_order == 1) return make_symbol("pde`UX");
        if (r->time_order == 0 && r->space_order == 2) return make_symbol("pde`UXX");
        if (r->time_order == 1 && r->space_order == 0 && time_order == 2) return make_symbol("pde`UT");
        throw Decline();
    }
    if (e->is_symbol(l.u)) throw Decline();
    if (!e->is_normal()) return e;
    ExprList args;
    for (const auto& a : e->args()) args.push_back(slot_form(a, l, x, t, time_order));
    return make_normal(slot_form(e->head(), l, x, t, time_order), std::move(args));
}

// Boundary condition at one end: u = value(t) (Dirichlet) or du/dx = value(t) (Neumann).
struct Boundary {
    bool neumann = false;
    ExprPtr value;  // expression in t
    Compiled f, df, d2f;
};

struct Pde {
    int order = 0;  // in time: 1 (heat, advection) or 2 (wave)
    double x0 = 0, x1 = 0, t0 = 0, t1 = 0, h = 0;
    Compiled rhs;
    ExprPtr initial, initial_rate;  // u(x, t0), du/dt(x, t0): expressions in x
    Boundary left, right;
};

struct MolSystem {
    std::shared_ptr<const Pde> p;

    // State: u_0..u_N, then (order 2) v_0..v_N with v = du/dt.
    void operator()(const Vector& s, Vector& ds, double t) const {
        const Pde& q = *p;
        const std::size_t n = kIntervals + 1;
        double slots[kSlots];
        slots[T] = t;
        const auto u = [&](std::size_t i) { return s[i]; };
        for (std::size_t i = 0; i < n; ++i) {
            const Boundary* b = i == 0 ? &q.left : i == n - 1 ? &q.right : nullptr;
            double rate;
            if (b && !b->neumann) {  // Dirichlet node follows the boundary value
                if (q.order == 1) {
                    ds[i] = b->df.eval(&t);
                } else {
                    ds[i] = s[n + i];
                    ds[n + i] = b->d2f.eval(&t);
                }
                continue;
            }
            double ux, uxx;
            if (i == 0) {  // ghost node u_{-1} = u_1 - 2 h g
                const double g = b->f.eval(&t);
                ux = g;
                uxx = 2 * (u(1) - u(0) - q.h * g) / (q.h * q.h);
            } else if (i == n - 1) {  // ghost node u_{N+1} = u_{N-1} + 2 h g
                const double g = b->f.eval(&t);
                ux = g;
                uxx = 2 * (u(n - 2) - u(n - 1) + q.h * g) / (q.h * q.h);
            } else {
                ux = (u(i + 1) - u(i - 1)) / (2 * q.h);
                uxx = (u(i + 1) - 2 * u(i) + u(i - 1)) / (q.h * q.h);
            }
            slots[X] = q.x0 + q.h * static_cast<double>(i);
            slots[U] = u(i);
            slots[UX] = ux;
            slots[UXX] = uxx;
            slots[UT] = q.order == 2 ? s[n + i] : 0.0;
            rate = q.rhs.eval(slots);
            if (!std::isfinite(rate)) throw numeric::NonFinite();
            if (q.order == 1) {
                ds[i] = rate;
            } else {
                ds[i] = s[n + i];
                ds[n + i] = rate;
            }
        }
    }
};

}  // namespace

std::optional<BackendResult> solve_pde(const ExprPtr& expr, std::string& method) {
    try {
        if (!expr || expr->size() != 4 || expr->arg(1)->has_head("List")) return std::nullopt;
        ExprPtr fn = expr->arg(1);
        if (fn->is_normal() && fn->size() == 2) fn = fn->head();  // u[x, t] -> u
        if (!fn->is_symbol()) return std::nullopt;
        const ExprList equations = expr->arg(0)->has_head("List") ? expr->arg(0)->args() : ExprList{expr->arg(0)};

        // The PDE: Derivative[{..}][u][a, b] == rhs with exactly one nonzero (time) order.
        Layout layout{fn->name(), 0};
        ExprPtr vars[2];
        std::optional<std::size_t> pde_index;
        int order = 0;
        for (std::size_t k = 0; k < equations.size() && !pde_index; ++k) {
            const auto& eq = equations[k];
            if (!eq->has_head("Equal") || eq->size() != 2) return std::nullopt;
            const auto& d = eq->arg(0);
            if (!d->is_normal() || d->size() != 2 || !d->head()->is_normal() || !d->head()->arg(0)->is_symbol(fn->name()) ||
                !d->head()->head()->has_head("Derivative") || !d->arg(0)->is_symbol() || !d->arg(1)->is_symbol())
                continue;
            const ExprPtr& o = d->head()->head()->arg(0);
            if (!o->has_head("List") || o->size() != 2 || !o->arg(0)->is_integer() || !o->arg(1)->is_integer())
                return std::nullopt;
            const bool first = !o->arg(0)->integer().is_zero(), second = !o->arg(1)->integer().is_zero();
            if (first == second) return std::nullopt;
            layout.space_index = first ? 1 : 0;  // the differentiated argument is time
            const auto ord = o->arg(first ? 0 : 1)->integer().to_int64();
            if (!ord || *ord < 1 || *ord > 2) return std::nullopt;
            order = static_cast<int>(*ord);
            vars[0] = d->arg(layout.space_index);
            vars[1] = d->arg(1 - layout.space_index);
            pde_index = k;
        }
        if (!pde_index) return std::nullopt;
        const ExprPtr &x = vars[0], &t = vars[1];

        auto p = std::make_shared<Pde>();
        p->order = order;
        bool have_x = false, have_t = false;
        for (std::size_t k = 2; k < 4; ++k) {
            const auto& r = expr->arg(k);
            if (!r->has_head("List") || r->size() != 3) return std::nullopt;
            const double lo = constant(r->arg(1)), hi = constant(r->arg(2));
            if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi)) return std::nullopt;
            if (equal(r->arg(0), x)) { p->x0 = lo; p->x1 = hi; have_x = true; }
            else if (equal(r->arg(0), t)) { p->t0 = lo; p->t1 = hi; have_t = true; }
        }
        if (!have_x || !have_t) return std::nullopt;
        p->h = (p->x1 - p->x0) / static_cast<double>(kIntervals);

        const numeric::Slots slots = {{x->name(), X}, {t->name(), T}, {"pde`U", U},
                                      {"pde`UX", UX}, {"pde`UXX", UXX}, {"pde`UT", UT}};
        p->rhs = compile(slot_form(equations[*pde_index]->arg(1), layout, x, t, order), slots);

        Boundary* ends[2] = {&p->left, &p->right};
        bool have_end[2] = {false, false};
        for (std::size_t k = 0; k < equations.size(); ++k) {
            if (k == *pde_index) continue;
            ExprPtr lhs = equations[k]->arg(0), rhs = equations[k]->arg(1);
            auto r = reference(lhs, layout);
            if (!r) {
                r = reference(rhs, layout);
                std::swap(lhs, rhs);
            }
            if (!r) return std::nullopt;
            if (equal(r->space_arg, x) && r->space_order == 0 && r->time_arg->is_number() &&
                constant(r->time_arg) == p->t0) {
                ExprPtr& slot = r->time_order == 0 ? p->initial : p->initial_rate;
                if (r->time_order >= order || slot) return std::nullopt;
                slot = rhs;
            } else if (equal(r->time_arg, t) && r->time_order == 0 && r->space_order <= 1 && r->space_arg->is_number()) {
                const double at = constant(r->space_arg);
                const int side = at == p->x0 ? 0 : at == p->x1 ? 1 : -1;
                if (side < 0 || have_end[side]) return std::nullopt;
                ends[side]->neumann = r->space_order == 1;
                ends[side]->value = rhs;
                have_end[side] = true;
            } else {
                return std::nullopt;
            }
        }
        if (!p->initial || (order == 2 && !p->initial_rate) || !have_end[0] || !have_end[1]) return std::nullopt;
        const numeric::Slots tslot = {{t->name(), 0}};
        for (Boundary* b : ends) {
            b->f = compile(b->value, tslot);
            const ExprPtr d1 = differentiate(b->value, t);
            b->df = compile(d1, tslot);
            b->d2f = compile(differentiate(d1, t), tslot);
        }

        const std::size_t n = kIntervals + 1;
        const Compiled g = compile(p->initial, {{x->name(), 0}});
        std::optional<Compiled> rate;
        if (order == 2) rate = compile(p->initial_rate, {{x->name(), 0}});
        Vector s0(order * n);
        std::vector<double> xs(n);
        for (std::size_t i = 0; i < n; ++i) {
            xs[i] = i == n - 1 ? p->x1 : p->x0 + p->h * static_cast<double>(i);
            s0[i] = g.eval(&xs[i]);
            if (order == 2) s0[n + i] = rate->eval(&xs[i]);
        }
        for (std::size_t side = 0; side < 2; ++side) {  // boundary values win at Dirichlet nodes
            const Boundary& b = *ends[side];
            if (b.neumann) continue;
            const std::size_t i = side == 0 ? 0 : n - 1;
            s0[i] = b.f.eval(&p->t0);
            if (order == 2) s0[n + i] = b.df.eval(&p->t0);
        }
        std::vector<double> ts(kTimes);
        for (std::size_t j = 0; j < kTimes; ++j)
            ts[j] = j == kTimes - 1 ? p->t1 : p->t0 + (p->t1 - p->t0) * static_cast<double>(j) / (kTimes - 1);

        const MolSystem system{p};
        const auto samples = solve_switching<MolSystem, NumericJacobian<MolSystem>>(
            system, [&] { return NumericJacobian<MolSystem>{system}; }, s0, ts, method);

        // Grid in u's argument order: values[j][i] = u(first[i], second[j]).
        const bool space_first = layout.space_index == 0;
        const std::vector<double>& first = space_first ? xs : ts;
        const std::vector<double>& second = space_first ? ts : xs;
        const auto value = [&](std::size_t xi, std::size_t tj) { return samples[tj][xi]; };
        const auto list = [](const std::vector<double>& v) {
            ExprList out;
            for (double d : v) out.push_back(numeric::decimal(d));
            return make_normal("List", std::move(out));
        };
        ExprList rows;
        for (std::size_t j = 0; j < second.size(); ++j) {
            ExprList row;
            for (std::size_t i = 0; i < first.size(); ++i)
                row.push_back(numeric::decimal(space_first ? value(i, j) : value(j, i)));
            rows.push_back(make_normal("List", std::move(row)));
        }
        const auto domain = [&](const std::vector<double>& v) {
            return make_normal("List", {numeric::decimal(v.front()), numeric::decimal(v.back())});
        };
        ExprPtr result = make_normal("InterpolatingFunction",
                                     {make_normal("List", {domain(first), domain(second)}), list(first),
                                      list(second), make_normal("List", std::move(rows))});
        return BackendResult{std::move(result), ResultStatus::Numeric, "odeint"};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace symats
