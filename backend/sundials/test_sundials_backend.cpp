// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "sundials_backend.h"

#include <cmath>

#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("SUNDIALS backend declines unsupported NDSolve shapes") {
    SundialsBackend backend;
    CHECK(backend.supports("NDSolve"));
    CHECK(!backend.evaluate(parse_text("NDSolve[{y'[x]==-y[x]},y,{x,0,1}]")));
    CHECK(!backend.evaluate(parse_text("NDSolve[{y'[x]==-y[x],y[0]==1},y,{x,1,0}]")));
}

TEST_CASE("SUNDIALS backend solves scalar first-order NDSolve") {
    SundialsBackend backend;
    const auto result = backend.evaluate(
        parse_text("NDSolve[{y'[x]==-y[x],y[0]==1},y,{x,0,1}]"));
    CHECK(result.has_value());
    if (result) {
        CHECK(result->status == ResultStatus::Numeric);
        CHECK(result->backend == "sundials");
        CHECK(result->value->has_head("InterpolatingFunction"));
        CHECK(result->value->size() == 2);
        CHECK(result->value->arg(1)->has_head("List"));
        CHECK(result->value->arg(1)->size() == 101);
    }
}

namespace {
// Value of the last sample of an InterpolatingFunction result.
double final_value(const BackendResult& result) {
    const auto& points = result.value->arg(1);
    return points->arg(points->size() - 1)->arg(1)->number().to_double();
}
}  // namespace

TEST_CASE("SUNDIALS backend NDSolve matches exact solutions") {
    SundialsBackend backend;
    const auto decay = backend.evaluate(
        parse_text("NDSolve[{y'[x]==-y[x],y[0]==1},y,{x,0,1}]"));
    CHECK(decay.has_value());
    if (decay) CHECK(std::abs(final_value(*decay) - std::exp(-1.0)) < 1e-6);

    // y' = Sin[y], y(0) = 1  =>  Tan[y/2] = Tan[1/2] E^x.
    const auto sine = backend.evaluate(
        parse_text("NDSolve[{y'[x]==Sin[y[x]],y[0]==1},y,{x,0,1}]"));
    CHECK(sine.has_value());
    if (sine)
        CHECK(std::abs(final_value(*sine) - 2.0 * std::atan(std::tan(0.5) * std::exp(1.0))) < 1e-6);
}
