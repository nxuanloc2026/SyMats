// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "quadrature_backend.h"

#include <cmath>
#include <memory>

#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
const double kPi = std::acos(-1.0);

std::optional<double> integral(QuadratureBackend& backend, const char* text) {
    const auto r = backend.evaluate(parse_text(text));
    if (!r || r->status != ResultStatus::Numeric || r->backend != "quadrature" || !r->value->is_number())
        return std::nullopt;
    return r->value->number().to_double();
}

bool near(std::optional<double> v, double expected, double tol) {
    return v && std::abs(*v - expected) <= tol;
}
}  // namespace

TEST_CASE("Quadrature backend declines unsupported NIntegrate forms") {
    QuadratureBackend b;
    CHECK(b.supports("NIntegrate"));
    CHECK(!b.supports("Integrate"));
    CHECK(!integral(b, "NIntegrate[a*x, {x, 0, 1}]"));                    // free parameter
    CHECK(!integral(b, "NIntegrate[x, x]"));                              // no range
    CHECK(!integral(b, "NIntegrate[x*y, {x, 0, y}, {y, 0, 1}]"));         // outer limit uses inner var
    CHECK(!integral(b, "NIntegrate[x, {x, 0, 1}, {x, 0, 1}]"));           // repeated variable
    CHECK(!integral(b, "NIntegrate[f[x], {x, 0, 1}]"));                   // unknown function
    CHECK(!integral(b, "NIntegrate[x*y*z*w, {x, 0, Infinity}, {y, 0, 1}, {z, 0, 1}, {w, 0, 1}]"));
}

TEST_CASE("Quadrature backend: one-dimensional integrals") {
    QuadratureBackend b;
    CHECK(near(integral(b, "NIntegrate[x^2, {x, 0, 1}]"), 1.0 / 3, 1e-11));
    CHECK(b.last_method() == "DoubleExponential");
    CHECK(b.last_error() < 1e-9);
    CHECK(near(integral(b, "NIntegrate[Sin[x], {x, 0, Pi}]"), 2.0, 1e-11));
    CHECK(near(integral(b, "NIntegrate[x, {x, 1, 0}]"), -0.5, 1e-11));
    CHECK(near(integral(b, "NIntegrate[1/Sqrt[x], {x, 0, 1}]"), 2.0, 1e-9));       // endpoint singularity
    CHECK(near(integral(b, "NIntegrate[Log[x], {x, 0, 1}]"), -1.0, 1e-9));
    CHECK(near(integral(b, "NIntegrate[Exp[-x^2], {x, 0, Infinity}]"), std::sqrt(kPi) / 2, 1e-10));
    CHECK(near(integral(b, "NIntegrate[Exp[-x^2], {x, -Infinity, Infinity}]"), std::sqrt(kPi), 1e-10));
    CHECK(near(integral(b, "NIntegrate[1/(1 + x^2), {x, -Infinity, 0}]"), kPi / 2, 1e-10));
    CHECK(near(integral(b, "NIntegrate[BesselJ[0, x], {x, 0, 1}]"), 0.919730410089760, 1e-11));
    CHECK(near(integral(b, "NIntegrate[Gamma[x], {x, 1, 2}]"), 0.92274595067959, 1e-10));
    CHECK(near(integral(b, "NIntegrate[x, {x, 2, 2}]"), 0.0, 0.0));
}

TEST_CASE("Quadrature backend: iterated integrals with variable limits") {
    QuadratureBackend b;
    // Integrate[x*y, {x, 0, 1}, {y, 0, x}] = 1/8.
    CHECK(near(integral(b, "NIntegrate[x*y, {x, 0, 1}, {y, 0, x}]"), 0.125, 1e-9));
    // Area of the unit disk.
    CHECK(near(integral(b, "NIntegrate[1, {x, -1, 1}, {y, -Sqrt[1 - x^2], Sqrt[1 - x^2]}]"), kPi, 1e-7));
    CHECK(near(integral(b, "NIntegrate[Exp[-x^2 - y^2], {x, -Infinity, Infinity}, {y, -Infinity, Infinity}]"),
               kPi, 1e-8));
    CHECK(near(integral(b, "NIntegrate[x*y*z, {x, 0, 1}, {y, 0, 1}, {z, 0, 1}]"), 0.125, 1e-7));
    // Volume of the tetrahedron x, y, z >= 0, x + y + z <= 1.
    CHECK(near(integral(b, "NIntegrate[1, {x, 0, 1}, {y, 0, 1 - x}, {z, 0, 1 - x - y}]"), 1.0 / 6, 1e-7));
}

TEST_CASE("Quadrature backend: VEGAS Monte Carlo in four and more dimensions") {
    QuadratureBackend b;
    const double g = std::sqrt(kPi) / 2 * std::erf(1.0);
    auto v = integral(b, "NIntegrate[Exp[-x^2 - y^2 - z^2 - w^2], {x, 0, 1}, {y, 0, 1}, {z, 0, 1}, {w, 0, 1}]");
    CHECK(b.last_method() == "Vegas");
    CHECK(near(v, std::pow(g, 4), 1e-3));
    CHECK(b.last_error() < 1e-3);
    // A sharp peak: the adaptive grid must find it. Exact value Pi^2/10^4 (erf(5) ~ 1).
    v = integral(b, "NIntegrate[Exp[-100*((x - 1/2)^2 + (y - 1/2)^2 + (z - 1/2)^2 + (w - 1/2)^2)],"
                    " {x, 0, 1}, {y, 0, 1}, {z, 0, 1}, {w, 0, 1}]");
    CHECK(v && std::abs(*v / (kPi * kPi / 1e4) - 1.0) < 1e-2);
    // Variable limits: volume of the 4-simplex = 1/24.
    v = integral(b, "NIntegrate[1, {x, 0, 1}, {y, 0, 1 - x}, {z, 0, 1 - x - y}, {w, 0, 1 - x - y - z}]");
    CHECK(near(v, 1.0 / 24, 2e-3));
    // Deterministic: the same input gives the same output.
    CHECK(integral(b, "NIntegrate[x*y*z*w, {x, 0, 1}, {y, 0, 1}, {z, 0, 1}, {w, 0, 1}]") ==
          integral(b, "NIntegrate[x*y*z*w, {x, 0, 1}, {y, 0, 1}, {z, 0, 1}, {w, 0, 1}]"));
}

TEST_CASE("Quadrature backend through the evaluator") {
    Context ctx;
    ctx.backends().add(std::make_shared<QuadratureBackend>());
    const auto r = evaluate_top(parse_text("NIntegrate[Exp[-x^2], {x, 0, Infinity}]"), ctx);
    CHECK(r.status == ResultStatus::Numeric);
    CHECK(r.value->is_number());
    if (r.value->is_number()) CHECK(std::abs(r.value->number().to_double() - std::sqrt(kPi) / 2) < 1e-10);
}
