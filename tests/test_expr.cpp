// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Expected values follow the canonical-form rules in docs/EXPR_SPEC.md §2.
#include "symats/expr.h"
#include "test.h"

using namespace symats;

namespace {
std::string F(const Ex& e) { return e.str(); }
const Ex x = sym("x");
const Ex y = sym("y");
const Ex a = sym("a");
const Ex b = sym("b");
const Ex c = sym("c");
}  // namespace

TEST_CASE("Expr: atoms and full form") {
    CHECK_EQ(F(Ex(42)), std::string("42"));
    CHECK_EQ(F(frac(6, 4)), std::string("3/2"));
    CHECK_EQ(F(frac(4, 2)), std::string("2"));
    CHECK(frac(4, 2)->is_integer());
    CHECK_EQ(F(x), std::string("x"));
    Ex integral = make_normal("Integrate", {pow(x, 2).ptr(), x.ptr()});
    CHECK_EQ(F(integral), std::string("Integrate[Power[x, 2], x]"));
    CHECK(integral->has_head("Integrate"));
}

TEST_CASE("Plus: numbers combine, like terms collect, identities vanish") {
    CHECK_EQ(F(x + x), std::string("Times[2, x]"));
    CHECK_EQ(F(Ex(2) + x + 3), std::string("Plus[5, x]"));
    CHECK_EQ(F(y + x + 1), std::string("Plus[1, x, y]"));
    CHECK_EQ(F(x - x), std::string("0"));
    CHECK_EQ(F(x + 0), std::string("x"));
    CHECK_EQ(F(2 * x + 3 * x), std::string("Times[5, x]"));
    CHECK_EQ(F(x * y + y * x), std::string("Times[2, x, y]"));
    CHECK_EQ(F(frac(1, 2) + frac(1, 2)), std::string("1"));
    CHECK_EQ(F(frac(1, 2) + x + frac(1, 3)), std::string("Plus[5/6, x]"));
}

TEST_CASE("Plus: flattening and ordering") {
    Ex nested = plus(ExprList{a, make_normal("Plus", {b.ptr(), c.ptr()})});
    CHECK_EQ(F(nested), std::string("Plus[a, b, c]"));
    CHECK_EQ(F(pow(x, 2) + x), std::string("Plus[x, Power[x, 2]]"));
    // Terms are ordered ignoring their numeric coefficient.
    CHECK_EQ(F(2 * y + x), std::string("Plus[x, Times[2, y]]"));
}

TEST_CASE("Plus: sums cancel") {
    CHECK_EQ(F((x + 1) - (x + 1)), std::string("0"));
    CHECK_EQ(F((x - y) + (y - x)), std::string("0"));
    // A numeric multiple of a sum stays factored on its own, and is distributed inside sums.
    CHECK_EQ(F(2 * (x + 1)), std::string("Times[2, Plus[1, x]]"));
    CHECK_EQ(F(y + 2 * (x + 1)), std::string("Plus[2, Times[2, x], y]"));
}

TEST_CASE("Canonical form does not depend on grouping") {
    const Ex s = x + 1;
    CHECK((15 * s) * y == 15 * (s * y));
    CHECK(F((15 * s) * y) == F(times(ExprList{Ex(15), s, y})));
    CHECK((x + 2 * s) + y == x + (2 * s + y));
}

TEST_CASE("Times: powers collect, identities vanish") {
    CHECK_EQ(F(x * x), std::string("Power[x, 2]"));
    CHECK_EQ(F(x * x * x), std::string("Power[x, 3]"));
    CHECK_EQ(F(x * 0), std::string("0"));
    CHECK_EQ(F(x * 1), std::string("x"));
    CHECK_EQ(F((x * y) / x), std::string("y"));
    CHECK_EQ(F(x / x), std::string("1"));
    CHECK_EQ(F(y * x * 3), std::string("Times[3, x, y]"));
    CHECK_EQ(F(pow(x, 2) * pow(x, -2)), std::string("1"));
    CHECK_EQ(F(x / y), std::string("Times[x, Power[y, -1]]"));
}

TEST_CASE("Power: exact numbers") {
    CHECK_EQ(F(pow(Ex(2), 100)), std::string("1267650600228229401496703205376"));
    CHECK_EQ(F(pow(Ex(2), -3)), std::string("1/8"));
    CHECK_EQ(F(pow(4, frac(1, 2))), std::string("2"));
    CHECK_EQ(F(pow(8, frac(1, 3))), std::string("2"));
    CHECK_EQ(F(pow(frac(8, 27), frac(2, 3))), std::string("4/9"));
    CHECK_EQ(F(pow(2, frac(1, 2))), std::string("Power[2, 1/2]"));
    CHECK_EQ(F(pow(2, frac(1, 2)) * pow(2, frac(1, 2))), std::string("2"));
    CHECK_EQ(F(pow(-8, frac(1, 3))), std::string("Power[-8, 1/3]"));  // principal value is complex
    CHECK_EQ(F(pow(Ex(0), 0)), std::string("Indeterminate"));
    CHECK_EQ(F(pow(Ex(0), -1)), std::string("ComplexInfinity"));
    CHECK_EQ(F(pow(1, x)), std::string("1"));
}

TEST_CASE("Power: symbolic rules") {
    CHECK_EQ(F(pow(x, 0)), std::string("1"));
    CHECK_EQ(F(pow(x, 1)), std::string("x"));
    CHECK_EQ(F(pow(pow(x, 2), 3)), std::string("Power[x, 6]"));
    CHECK_EQ(F(pow(2 * x, 2)), std::string("Times[4, Power[x, 2]]"));
    CHECK_EQ(F(pow(x * y, -1)), std::string("Times[Power[x, -1], Power[y, -1]]"));
    // (x^2)^(1/2) is NOT simplified to x (wrong for negative x).
    CHECK_EQ(F(pow(pow(x, 2), frac(1, 2))), std::string("Power[Power[x, 2], 1/2]"));
}

TEST_CASE("Equality, hashing and canonical order") {
    Ex s1 = x + y, s2 = y + x;
    CHECK(s1 == s2);
    CHECK(s1->hash() == s2->hash());
    CHECK(!(x + y == x - y));
    CHECK(compare(x, pow(x, 2)) < 0);
    CHECK(compare(pow(x, 2), pow(x, 3)) < 0);
    CHECK(compare(pow(x, 3), y) < 0);
    CHECK(compare(Ex(5), x) < 0);
    CHECK(compare(x, x) == 0);
}
