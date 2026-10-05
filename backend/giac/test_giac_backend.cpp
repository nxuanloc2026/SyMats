// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac_backend.h"

#include "test.h"

using namespace symats;

TEST_CASE("Giac bridge preserves exact atoms and lists") {
    std::cerr << "atoms start\n";
    const ExprList cases = {
        make_integer(Integer::from_string("123456789012345678901234567890")),
        make_rational(7, 13), make_symbol("x"), make_symbol("Pi"),
        make_normal("List", {make_integer(1), make_symbol("y")}),
    };
    for (const auto& expr : cases) {
        std::cerr << "atom " << to_full_form(expr) << "\n";
        auto result = giac_roundtrip(expr);
        CHECK(result.has_value());
        if (result) CHECK(equal(*result, expr));
    }
}

TEST_CASE("Giac bridge preserves mapped and generic heads") {
    std::cerr << "heads start\n";
    const auto x = make_symbol("x");
    const ExprList cases = {
        plus({make_integer(2), x}),
        make_normal("Sin", {x}),
        make_normal("f", {x, make_integer(3)}),
        make_normal(make_normal("Derivative", {make_integer(1)}), {x}),
    };
    for (const auto& expr : cases) {
        auto result = giac_roundtrip(expr);
        CHECK(result.has_value());
        if (result) CHECK(equal(*result, expr));
    }
}

TEST_CASE("Giac backend supports symbolic factorization") {
    std::cerr << "factor start\n";
    GiacBackend backend;
    CHECK(backend.supports("Factor"));
    CHECK(!backend.supports("Plot"));
    const auto x = make_symbol("x");
    const auto polynomial = subtract(power(x, make_integer(2)), make_integer(1));
    auto result = backend.evaluate(make_normal("Factor", {polynomial}));
    CHECK(result.has_value());
    if (result) {
        CHECK(result->value != nullptr);
        CHECK(result->status == ResultStatus::Unverified);
    }
}
