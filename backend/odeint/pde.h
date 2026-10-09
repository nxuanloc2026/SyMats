// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <optional>
#include <string>

#include "symats/backend.h"

namespace symats {

// NDSolve[{pde, initial conditions, boundary conditions}, u, {x, x0, x1}, {t, t0, t1}]
// for u[x, t] (either argument order) with
//   D[u[x, t], t] == f[x, t, u, D[u, x], D[u, {x, 2}]]                   (first order in time)
//   D[u[x, t], {t, 2}] == f[x, t, u, D[u, x], D[u, {x, 2}], D[u, t]]     (second order)
// initial values u[x, t0] == g[x] (and D[u[x, t], t] /. t -> t0 for order 2, written
// Derivative[{0, 1}][u][x, t0] == h[x]), and at each end u[x0, t] == a[t] (Dirichlet) or
// Derivative[{1, 0}][u][x0, t] == a[t] (Neumann). Method of lines on 51 nodes.
// Result: InterpolatingFunction[{{x0, x1}, {t0, t1}}, {xs}, {ts}, {{u, ...}, ...}] in u's
// argument order (one row per value of the second argument). `method` gets the integrator.
std::optional<BackendResult> solve_pde(const ExprPtr& expr, std::string& method);

}  // namespace symats
