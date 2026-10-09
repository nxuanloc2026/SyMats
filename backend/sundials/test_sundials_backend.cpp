// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "sundials_backend.h"

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
