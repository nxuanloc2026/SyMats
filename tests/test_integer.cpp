// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/integer.h"
#include "test.h"

using symats::Integer;

namespace {
Integer I(const char* s) { return Integer::from_string(s); }
Integer factorial(int n) {
    Integer r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}
}  // namespace

TEST_CASE("Integer: construction and printing") {
    CHECK_EQ(Integer(0).to_string(), std::string("0"));
    CHECK_EQ(Integer(-42).to_string(), std::string("-42"));
    CHECK_EQ(Integer(1000000000).to_string(), std::string("1000000000"));
    CHECK_EQ(Integer(9223372036854775807LL).to_string(), std::string("9223372036854775807"));
    CHECK_EQ(Integer(-9223372036854775807LL - 1).to_string(), std::string("-9223372036854775808"));
    CHECK_EQ(I("-000123").to_string(), std::string("-123"));
    CHECK_EQ(I("-0").to_string(), std::string("0"));
    CHECK_EQ(I("123456789012345678901234567890").to_string(),
             std::string("123456789012345678901234567890"));
    CHECK_THROWS(I(""));
    CHECK_THROWS(I("12a"));
}

TEST_CASE("Integer: addition and subtraction with carries and signs") {
    CHECK_EQ(I("999999999999999999") + 1, I("1000000000000000000"));
    CHECK_EQ(I("1000000000000000000") - 1, I("999999999999999999"));
    CHECK_EQ(Integer(5) - 8, Integer(-3));
    CHECK_EQ(Integer(-5) + 8, Integer(3));
    CHECK_EQ(Integer(-5) - (-5), Integer(0));
    CHECK(!(Integer(-5) - (-5)).is_negative());
}

TEST_CASE("Integer: multiplication, powers and factorials") {
    CHECK_EQ(Integer::pow(2, 100), I("1267650600228229401496703205376"));
    CHECK_EQ(factorial(30), I("265252859812191058636308480000000"));
    CHECK_EQ(Integer(-3) * 4, Integer(-12));
    CHECK_EQ(Integer(-3) * -4, Integer(12));
    CHECK_EQ(Integer::pow(-2, 3), Integer(-8));
    CHECK_EQ(Integer::pow(7, 0), Integer(1));
}

TEST_CASE("Integer: division and remainder (truncated)") {
    CHECK_EQ(Integer(7) / 2, Integer(3));
    CHECK_EQ(Integer(-7) / 2, Integer(-3));
    CHECK_EQ(Integer(-7) % 2, Integer(-1));
    CHECK_EQ(Integer(7) % -2, Integer(1));
    CHECK_THROWS(Integer(1) / 0);

    // Multi-limb divisor: 30!/20! = 21*22*...*30.
    Integer p = 1;
    for (int i = 21; i <= 30; ++i) p *= i;
    CHECK_EQ(factorial(30) / factorial(20), p);
    CHECK((factorial(30) % factorial(20)).is_zero());

    // Identity a = q*b + r on large numbers.
    Integer a = Integer::pow(3, 200) + 12345;
    Integer b = Integer::pow(7, 40) - 1;
    auto [q, r] = Integer::divmod(a, b);
    CHECK_EQ(q * b + r, a);
    CHECK(r.abs() < b.abs());
}

TEST_CASE("Integer: gcd, roots, comparisons, conversion") {
    CHECK_EQ(Integer::gcd(48, 180), Integer(12));
    CHECK_EQ(Integer::gcd(-48, 180), Integer(12));
    CHECK_EQ(Integer::gcd(0, 5), Integer(5));
    CHECK_EQ(Integer::gcd(factorial(25), Integer::pow(2, 40)), Integer::pow(2, 22));

    auto [r1, e1] = Integer(1000000).iroot(2);
    CHECK_EQ(r1, Integer(1000));
    CHECK(e1);
    auto [r2, e2] = Integer(1000001).iroot(2);
    CHECK_EQ(r2, Integer(1000));
    CHECK(!e2);
    auto [r3, e3] = Integer::pow(12345, 7).iroot(7);
    CHECK_EQ(r3, Integer(12345));
    CHECK(e3);

    CHECK(Integer(-10) < Integer(3));
    CHECK(Integer(-10) < Integer(-3));
    CHECK(I("100000000000000000000") > I("99999999999999999999"));

    CHECK(Integer(123).to_int64().value() == 123);
    CHECK(!Integer::pow(10, 30).to_int64().has_value());
}

TEST_CASE("Integer: algebraic property tests") {
    // Large integer properties
    Integer x = I("987654321098765432109876543210");
    Integer y = I("123456789012345678901234567890");

    CHECK_EQ((x + y) - y, x);
    CHECK_EQ((x * y) / y, x);
    CHECK_EQ((x * y) % y, Integer(0));
    CHECK_EQ(Integer::gcd(x, y) * Integer::gcd(x + 1, y + 1) > 0, true);
    CHECK(x.is_even());
    CHECK(!y.abs().is_negative());
}
