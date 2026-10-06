// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
void round_trip(std::string_view input) {
    ExprPtr first = parse_text(input);
    CHECK(equal(first, parse_text(to_text(first))));
}
}  // namespace

TEST_CASE("Text: arithmetic precedence and exact numbers") {
    CHECK_EQ(to_full_form(parse_text("2x + 3*x")), std::string("Times(5, x)"));
    CHECK_EQ(to_full_form(parse_text("-x^2")), std::string("Times(-1, Power(x, 2))"));
    CHECK_EQ(to_full_form(parse_text("x^-2")), std::string("Power(x, -2)"));
    CHECK_EQ(to_full_form(parse_text("1/2 + 1/3")), std::string("5/6"));
    CHECK_EQ(to_full_form(parse_text("2^3^2")), std::string("512"));
    CHECK_EQ(to_full_form(parse_text("a b")), std::string("Times(a, b)"));
    CHECK_EQ(to_full_form(parse_text("123456789012345678901234567890")),
             std::string("123456789012345678901234567890"));
}

TEST_CASE("Text: calls, matrices, and internal form") {
    CHECK_EQ(to_full_form(parse_text("sin(x) + sqrt(4)")),
             std::string("Plus(2, Sin(x))"));
    CHECK_EQ(to_full_form(parse_text("integrate(x^2, x, 0, 1)")),
             std::string("Integrate(Power(x, 2), List(x, 0, 1))"));
    CHECK_EQ(to_full_form(parse_text("[[a,b],[c,d]]")),
             std::string("List(List(a, b), List(c, d))"));
    CHECK_EQ(to_full_form(parse_text("Plus(1, Times(2, x))")),
             std::string("Plus(1, Times(2, x))"));
    CHECK_EQ(to_full_form(parse_text("limit(x^2, x -> 0)")),
             std::string("Limit(Power(x, 2), Rule(x, 0))"));
    CHECK_EQ(to_full_form(parse_text("x^2 == 1")),
             std::string("Equal(Power(x, 2), 1)"));
}

TEST_CASE("Text: stable print and parse") {
    round_trip("x^2 + 2*x + 1");
    round_trip("integrate(sin(x), x, 0, pi)");
    round_trip("[[a, 1/2], [c, d]]");
    round_trip("f(x, y) + x^(1/2)");
    round_trip("(-8)^(1/3)");
    round_trip("`e` + e");
    round_trip("a b + c");
    round_trip("dsolve(y(x) == 0, y(x), x)");
    round_trip("sin(x) + `sin`");
    ExprPtr applied = make_normal(make_normal("f", {make_symbol("x")}), {make_symbol("y")});
    CHECK(equal(applied, parse_text(to_text(applied))));
    ExprPtr lower_head = make_normal("sin", {make_symbol("x")});
    CHECK(equal(lower_head, parse_text(to_text(lower_head))));
}

TEST_CASE("Text: rejects malformed input") {
    CHECK_THROWS(parse_text(""));
    CHECK_THROWS(parse_text("x +"));
    CHECK_THROWS(parse_text("sin(x"));
    CHECK_THROWS(parse_text("1 2"));
    CHECK_THROWS(parse_text("[a, b}"));
}

TEST_CASE("Text: readable output remains parseable") {
    CHECK_EQ(to_text(parse_text("x-y")), std::string("x - y"));
    CHECK_EQ(to_text(parse_text("-x^2")), std::string("-x^2"));
    CHECK_EQ(to_text(parse_text("sin(x)")), std::string("sin(x)"));
    CHECK_EQ(to_text(parse_text("x==2")), std::string("x == 2"));
    CHECK_EQ(to_text(parse_text("x->2")), std::string("x -> 2"));
    round_trip("x-y");
    round_trip("-2*x^2 + sin(x)");
    round_trip("x == 2");
    round_trip("x -> 2");
}

TEST_CASE("Text: exact decimals and assignment") {
    CHECK_EQ(to_full_form(parse_text("1.5")), std::string("3/2"));
    CHECK_EQ(to_full_form(parse_text(".125")), std::string("1/8"));
    CHECK_EQ(to_full_form(parse_text("12.00")), std::string("12"));
    CHECK_EQ(to_full_form(parse_text("x = 2")), std::string("Set(x, 2)"));
    CHECK_EQ(to_full_form(parse_text("x := 2")), std::string("SetDelayed(x, 2)"));
    CHECK_EQ(to_full_form(parse_text("x == 2")), std::string("Equal(x, 2)"));
    round_trip("1.5*x = 3.75");
    round_trip("f(x) := x^2");
    CHECK_THROWS(parse_text("1."));
}

TEST_CASE("Text: space before parenthesis means multiplication") {
    CHECK_EQ(to_full_form(parse_text("x (y+1)")),
             std::string("Times(x, Plus(1, y))"));
    CHECK_EQ(to_full_form(parse_text("x(y+1)")),
             std::string("x(Plus(1, y))"));
}

TEST_CASE("Text: multi-statement cells, output suppression, line continuation, and history") {
    // Multi-statement cells separated by semicolons or newlines
    CHECK_EQ(to_full_form(parse_text("x = 1; y = 2; x + y")),
             std::string("CompoundExpression(Set(x, 1), Set(y, 2), Plus(x, y))"));
    CHECK_EQ(to_full_form(parse_text("x = 1\ny = 2\nx + y")),
             std::string("CompoundExpression(Set(x, 1), Set(y, 2), Plus(x, y))"));

    // Trailing semicolon suppresses output by ending with Null
    CHECK_EQ(to_full_form(parse_text("x = 1;")),
             std::string("CompoundExpression(Set(x, 1), Null)"));
    CHECK_EQ(to_full_form(parse_text("x = 1; y = 2;")),
             std::string("CompoundExpression(Set(x, 1), Set(y, 2), Null)"));

    // Line continuation over pending binary operators
    CHECK_EQ(to_full_form(parse_text("x +\n y")),
             std::string("Plus(x, y)"));
    CHECK_EQ(to_full_form(parse_text("x -\n y")),
             std::string("Plus(x, Times(-1, y))"));

    // History references %, %%, %%%, %n, %-n
    CHECK_EQ(to_full_form(parse_text("%")), std::string("Out()"));
    CHECK_EQ(to_full_form(parse_text("%%")), std::string("Out(-2)"));
    CHECK_EQ(to_full_form(parse_text("%%%")), std::string("Out(-3)"));
    CHECK_EQ(to_full_form(parse_text("%1")), std::string("Out(1)"));
    CHECK_EQ(to_full_form(parse_text("%12")), std::string("Out(12)"));
    CHECK_EQ(to_full_form(parse_text("%-2")), std::string("Out(-2)"));

    CHECK_EQ(to_text(parse_text("%-2")), std::string("%%"));
    CHECK_EQ(to_text(parse_text("%%")), std::string("%%"));

    // Round-trip tests for multi-statement, output suppression, and history
    round_trip("a = 1; b = 2; a + b");
    round_trip("a = 1; b = 2;");
    round_trip("% + %1 + %%");
}

TEST_CASE("Text: nesting limit throws before stack exhaustion") {
    const std::string parens = std::string(400, '(') + "x" + std::string(400, ')');
    CHECK_THROWS(parse_text(parens));
    CHECK_THROWS(parse_text(std::string(400, '-') + "x"));
    ExprPtr deep = make_symbol("x");
    for (int i = 0; i < 300; ++i) deep = make_normal("f", {deep});
    CHECK_THROWS(to_text(deep));
}
