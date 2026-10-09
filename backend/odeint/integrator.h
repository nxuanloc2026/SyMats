// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Shared Boost.Odeint driver for the ODE and PDE (method of lines) solvers: Dormand-Prince
// 5(4) first, Rosenbrock 4 when the explicit method exceeds its step budget (stiffness).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/numeric/odeint.hpp>

#include "symats/backend.h"

namespace symats::odeint_detail {
namespace odeint = boost::numeric::odeint;
using Vector = boost::numeric::ublas::vector<double>;
using Matrix = boost::numeric::ublas::matrix<double>;

struct BudgetExceeded : std::runtime_error {
    BudgetExceeded() : std::runtime_error("NDSolve step budget exceeded") {}
};

// Counts steps over the whole integration (odeint's max_step_checker resets per sample).
struct StepBudget {
    std::size_t* used;
    std::size_t limit;
    void operator()() {
        if (++*used > limit) throw BudgetExceeded();
        if ((*used & 63) == 0 && backend_abort_requested()) throw std::runtime_error("aborted");
    }
    void reset() {}
};

constexpr double kAbsTol = 1e-10, kRelTol = 1e-10;
constexpr std::size_t kExplicitBudget = 20000, kStiffBudget = 200000;

// Integrates from x and records the state at each sample time.
template <class Stepper, class Sys>
std::vector<Vector> integrate(Stepper stepper, Sys sys, Vector x, const std::vector<double>& times,
                              std::size_t budget) {
    std::vector<Vector> samples;
    std::size_t used = 0;
    odeint::integrate_times(stepper, sys, x, times.begin(), times.end(),
                            (times.back() - times.front()) / 1000.0,
                            [&](const Vector& state, double) { samples.push_back(state); },
                            StepBudget{&used, budget});
    if (samples.size() != times.size()) throw std::runtime_error("NDSolve sampling failed");
    return samples;
}

// Finite-difference Jacobian (and time derivative) of a system, for Rosenbrock.
template <class Sys>
struct NumericJacobian {
    Sys system;
    void operator()(const Vector& x, Matrix& jac, double t, Vector& dfdt) const {
        const std::size_t n = x.size();
        Vector f0(n), f1(n), shifted = x;
        system(x, f0, t);
        jac.resize(n, n, false);
        for (std::size_t j = 0; j < n; ++j) {
            const double h = 1e-7 * std::max(1.0, std::abs(x[j]));
            shifted[j] = x[j] + h;
            system(shifted, f1, t);
            shifted[j] = x[j];
            for (std::size_t i = 0; i < n; ++i) jac(i, j) = (f1[i] - f0[i]) / h;
        }
        const double h = 1e-7 * std::max(1.0, std::abs(t));
        system(x, f1, t + h);
        dfdt.resize(n, false);
        for (std::size_t i = 0; i < n; ++i) dfdt[i] = (f1[i] - f0[i]) / h;
    }
};

// Dormand-Prince, then Rosenbrock with `jacobian` if the problem turns out stiff.
template <class Sys, class Jac>
std::vector<Vector> solve_switching(const Sys& system, const std::function<Jac()>& jacobian, const Vector& x0,
                                    const std::vector<double>& times, std::string& method) {
    try {
        method = "DormandPrince";
        return integrate(odeint::make_dense_output(kAbsTol, kRelTol, odeint::runge_kutta_dopri5<Vector>()),
                         system, x0, times, kExplicitBudget);
    } catch (const BudgetExceeded&) {
        method = "Rosenbrock";
        return integrate(odeint::make_dense_output(kAbsTol, kRelTol, odeint::rosenbrock4<double>()),
                         std::make_pair(system, jacobian()), x0, times, kStiffBudget);
    }
}

}  // namespace symats::odeint_detail
