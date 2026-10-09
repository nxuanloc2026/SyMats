// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// VEGAS adaptive Monte Carlo integration over the unit hypercube
// (G. P. Lepage, J. Comput. Phys. 27 (1978) 192). Independent implementation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace symats::numeric {

struct VegasOptions {
    std::size_t bins = 50;
    std::size_t iterations = 15;       // total, including warm-up
    std::size_t warmup = 5;            // iterations only used to adapt the grid
    std::size_t samples = 20000;       // per iteration
    double alpha = 1.5;                // grid adaptation damping
    std::uint64_t seed = 0x5eed5eedULL;
};

struct VegasResult {
    double value = 0.0;
    double error = 0.0;        // one standard deviation
    double chi2_per_dof = 0.0; // consistency of the combined iterations (about 1 is good)
};

// Integrates f over [0, 1]^dimension. f receives `dimension` coordinates.
VegasResult vegas(const std::function<double(const double*)>& f, std::size_t dimension,
                  const VegasOptions& options = {});

}  // namespace symats::numeric
