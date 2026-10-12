// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/vegas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

#include "symats/eval.h"

namespace symats {

VegasResult vegas_integrate(
    const std::function<double(const std::vector<double>&)>& integrand,
    std::size_t dimensions,
    const std::vector<std::pair<double, double>>& bounds,
    std::size_t samples_per_iter,
    std::size_t max_iterations) {

    if (dimensions == 0 || bounds.size() != dimensions) {
        return {0.0, 0.0, false};
    }

    constexpr std::size_t K = 50; // Number of grid intervals per dimension
    std::vector<std::vector<double>> x(dimensions, std::vector<double>(K + 1));

    // Initialize uniform grid in [0, 1] for each dimension
    for (std::size_t d = 0; d < dimensions; ++d) {
        for (std::size_t i = 0; i <= K; ++i) {
            x[d][i] = static_cast<double>(i) / static_cast<double>(K);
        }
    }

    // Compute volume element
    double volume = 1.0;
    for (std::size_t d = 0; d < dimensions; ++d) {
        volume *= (bounds[d].second - bounds[d].first);
    }

    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    double weighted_value_sum = 0.0;
    double weighted_var_sum = 0.0;

    for (std::size_t iter = 0; iter < max_iterations; ++iter) {
        double sum_f = 0.0;
        double sum_f2 = 0.0;
        std::vector<std::vector<double>> d_m(dimensions, std::vector<double>(K, 0.0));

        for (std::size_t s = 0; s < samples_per_iter; ++s) {
            std::vector<double> point(dimensions);
            std::vector<std::size_t> bin_indices(dimensions);
            double jacobian = volume;

            for (std::size_t d = 0; d < dimensions; ++d) {
                double u = dist(rng);
                std::size_t k = static_cast<std::size_t>(u * K);
                if (k >= K) k = K - 1;
                bin_indices[d] = k;

                double bin_width = x[d][k + 1] - x[d][k];
                double sub_u = dist(rng);
                double y_d = x[d][k] + sub_u * bin_width;

                point[d] = bounds[d].first + y_d * (bounds[d].second - bounds[d].first);
                jacobian *= bin_width * K;
            }

            double f_val = integrand(point);
            double weighted_f = f_val * jacobian;

            sum_f += weighted_f;
            sum_f2 += weighted_f * weighted_f;

            for (std::size_t d = 0; d < dimensions; ++d) {
                d_m[d][bin_indices[d]] += weighted_f * weighted_f;
            }
        }

        double mean_iter = sum_f / samples_per_iter;
        double var_iter = (sum_f2 / samples_per_iter - mean_iter * mean_iter) / samples_per_iter;
        if (var_iter <= 0.0) var_iter = 1e-15;

        double weight = 1.0 / var_iter;
        weighted_value_sum += mean_iter * weight;
        weighted_var_sum += weight;

        // Refine grid interval widths for next iteration
        constexpr double alpha = 1.5;
        for (std::size_t d = 0; d < dimensions; ++d) {
            std::vector<double> r(K, 0.0);
            double r_sum = 0.0;
            for (std::size_t k = 0; k < K; ++k) {
                double avg_f2 = d_m[d][k] / samples_per_iter;
                r[k] = std::pow((avg_f2 > 0.0 ? avg_f2 : 1e-15), alpha);
                r_sum += r[k];
            }

            if (r_sum > 0.0) {
                std::vector<double> new_x(K + 1, 0.0);
                new_x[0] = 0.0;
                new_x[K] = 1.0;
                double acc = 0.0;
                std::size_t current_k = 0;
                double step = r_sum / K;

                for (std::size_t i = 1; i < K; ++i) {
                    double target = i * step;
                    while (current_k < K && acc + r[current_k] < target) {
                        acc += r[current_k];
                        current_k++;
                    }
                    if (current_k < K && r[current_k] > 0.0) {
                        double frac = (target - acc) / r[current_k];
                        new_x[i] = x[d][current_k] + frac * (x[d][current_k + 1] - x[d][current_k]);
                    } else {
                        new_x[i] = static_cast<double>(i) / K;
                    }
                }
                x[d] = new_x;
            }
        }
    }

    double final_val = weighted_var_sum > 0.0 ? (weighted_value_sum / weighted_var_sum) : 0.0;
    double final_err = weighted_var_sum > 0.0 ? std::sqrt(1.0 / weighted_var_sum) : 0.0;

    return {final_val, final_err, true};
}

namespace {

double eval_numeric_expr(const ExprPtr& expr, const std::vector<std::string>& var_names, const std::vector<double>& vals, Context& ctx) {
    Context sub_ctx = ctx;
    for (std::size_t i = 0; i < var_names.size(); ++i) {
        constexpr std::int64_t scale = 1000000000000LL;
        auto num = make_rational(Integer(std::llround(vals[i] * scale)), Integer(scale));
        sub_ctx.set_value(var_names[i], num);
    }
    auto res = evaluate(expr, sub_ctx);
    if (res->is_number()) {
        return res->number().to_double();
    }
    return 0.0;
}

} // namespace

ExprPtr evaluate_nintegrate(const ExprPtr& expr, Context& ctx) {
    if (!expr || !expr->is_normal() || expr->size() < 2) {
        return expr;
    }

    ExprPtr func = expr->arg(0);
    std::vector<std::string> var_names;
    std::vector<std::pair<double, double>> bounds;

    for (std::size_t i = 1; i < expr->size(); ++i) {
        const auto& spec = expr->arg(i);
        if (spec->has_head("List") && spec->size() == 3 && spec->arg(0)->is_symbol()) {
            var_names.push_back(spec->arg(0)->name());
            double lower = evaluate(spec->arg(1), ctx)->number().to_double();
            double upper = evaluate(spec->arg(2), ctx)->number().to_double();
            bounds.push_back({lower, upper});
        } else {
            return expr;
        }
    }

    auto integrand = [&](const std::vector<double>& vals) -> double {
        return eval_numeric_expr(func, var_names, vals, ctx);
    };

    auto res = vegas_integrate(integrand, var_names.size(), bounds);
    if (!res.status_ok) {
        return expr;
    }

    constexpr std::int64_t scale = 1000000000000LL;
    return make_rational(Integer(std::llround(res.value * scale)), Integer(scale));
}

} // namespace symats
