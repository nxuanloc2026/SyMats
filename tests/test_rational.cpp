// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/rational.h"
#include "test.h"

using symats::Integer;
using symats::Rational;

TEST_CASE("Rational: normalization") {
    CHECK_EQ(Rational(6, -4).to_string(), std::string("-3/2"));
    CHECK_EQ(Rational(10, 5).to_string(), std::string("2"));
    CHECK(Rational(10, 5).is_integer());
    CHECK_EQ(Rational(0, -7).to_string(), std::string("0"));
    CHECK_THROWS(Rational(1, 0));
}

TEST_CASE("Rational: arithmetic") {
    CHECK_EQ(Rational(1, 2) + Rational(1, 3), Rational(5, 6));
    CHECK_EQ(Rational(1, 2) - Rational(1, 2), Rational(0));
    CHECK_EQ(Rational(2, 3) * Rational(9, 4), Rational(3, 2));
    CHECK_EQ(Rational(2, 3) / Rational(4, 9), Rational(3, 2));
    CHECK_EQ(Rational::pow(Rational(2, 3), -2), Rational(9, 4));
    CHECK_THROWS(Rational(0).reciprocal());
}

TEST_CASE("Rational: ordering and exact roots") {
    CHECK(Rational(1, 3) < Rational(1, 2));
    CHECK(Rational(-1, 2) < Rational(-1, 3));
    CHECK_EQ(Rational(8, 27).exact_root(3).value(), Rational(2, 3));
    CHECK_EQ(Rational(-8, 27).exact_root(3).value(), Rational(-2, 3));
    CHECK(!Rational(2).exact_root(2).has_value());
    CHECK(!Rational(-4).exact_root(2).has_value());
}
