// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "vegas.h"

#include "symats/backend.h"

#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace symats::numeric {
namespace {

// Moves the bin edges of one axis so that each bin carries equal (damped) weight.
void refine(std::vector<double>& edges, std::vector<double> d, double alpha) {
    const std::size_t n = d.size();
    if (n < 2) return;
    std::vector<double> smooth(n);
    smooth[0] = (d[0] + d[1]) / 2;
    smooth[n - 1] = (d[n - 2] + d[n - 1]) / 2;
    for (std::size_t i = 1; i + 1 < n; ++i) smooth[i] = (d[i - 1] + d[i] + d[i + 1]) / 3;
    double sum = 0.0;
    for (double v : smooth) sum += v;
    if (!(sum > 0.0) || !std::isfinite(sum)) return;
    std::vector<double> w(n, 0.0);
    double total = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double r = smooth[i] / sum;
        if (r > 0.0 && r < 1.0) w[i] = std::pow((r - 1.0) / std::log(r), alpha);
        else if (r >= 1.0) w[i] = 1.0;
        total += w[i];
    }
    if (!(total > 0.0)) return;
    std::vector<double> next(n + 1);
    next[0] = 0.0;
    next[n] = 1.0;
    double acc = 0.0;
    std::size_t j = 0;
    for (std::size_t k = 1; k < n; ++k) {
        const double target = total * static_cast<double>(k) / static_cast<double>(n);
        while (j < n - 1 && acc + w[j] < target) acc += w[j++];
        const double frac = w[j] > 0.0 ? (target - acc) / w[j] : 0.0;
        next[k] = edges[j] + std::min(1.0, std::max(0.0, frac)) * (edges[j + 1] - edges[j]);
    }
    edges = std::move(next);
}

}  // namespace

VegasResult vegas(const std::function<double(const double*)>& f, std::size_t dimension,
                  const VegasOptions& o) {
    if (dimension == 0 || o.bins == 0 || o.samples < 2 || o.iterations <= o.warmup)
        throw std::invalid_argument("bad VEGAS options");
    const std::size_t nb = o.bins;
    std::vector<std::vector<double>> edges(dimension, std::vector<double>(nb + 1));
    for (auto& e : edges)
        for (std::size_t i = 0; i <= nb; ++i) e[i] = static_cast<double>(i) / static_cast<double>(nb);

    std::mt19937_64 rng(o.seed);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::vector<double> x(dimension);
    std::vector<std::size_t> bin(dimension);
    double weight_sum = 0.0, weighted_value = 0.0, chi_acc = 0.0;
    std::vector<double> values, variances;

    for (std::size_t it = 0; it < o.iterations; ++it) {
        if (backend_abort_requested()) throw std::runtime_error("aborted");
        std::vector<std::vector<double>> d(dimension, std::vector<double>(nb, 0.0));
        double sum = 0.0, sum2 = 0.0;
        for (std::size_t s = 0; s < o.samples; ++s) {
            double jac = 1.0;
            for (std::size_t k = 0; k < dimension; ++k) {
                const double r = uniform(rng) * static_cast<double>(nb);
                const auto i = std::min(nb - 1, static_cast<std::size_t>(r));
                const double width = edges[k][i + 1] - edges[k][i];
                x[k] = edges[k][i] + (r - static_cast<double>(i)) * width;
                jac *= static_cast<double>(nb) * width;
                bin[k] = i;
            }
            const double fx = f(x.data()) * jac;
            if (!std::isfinite(fx)) throw std::domain_error("VEGAS integrand is not finite");
            sum += fx;
            sum2 += fx * fx;
            for (std::size_t k = 0; k < dimension; ++k) d[k][bin[k]] += fx * fx;
        }
        const double n = static_cast<double>(o.samples);
        const double mean = sum / n;
        const double variance = std::max((sum2 / n - mean * mean) / (n - 1.0), 1e-300);
        if (it >= o.warmup) {
            values.push_back(mean);
            variances.push_back(variance);
            weight_sum += 1.0 / variance;
            weighted_value += mean / variance;
        }
        for (std::size_t k = 0; k < dimension; ++k) refine(edges[k], d[k], o.alpha);
    }

    VegasResult result;
    result.value = weighted_value / weight_sum;
    result.error = std::sqrt(1.0 / weight_sum);
    for (std::size_t i = 0; i < values.size(); ++i)
        chi_acc += (values[i] - result.value) * (values[i] - result.value) / variances[i];
    result.chi2_per_dof = values.size() > 1 ? chi_acc / static_cast<double>(values.size() - 1) : 0.0;
    return result;
}

}  // namespace symats::numeric
