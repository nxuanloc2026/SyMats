// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <iostream>
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
void round_trip(std::string_view input) {
    try {
        ExprPtr first = parse_text(input);
        std::string printed = to_text(first);
        ExprPtr second = parse_text(printed);
        if (!equal(first, second)) {
            std::cout << "ROUND TRIP MISMATCH for '" << input << "': printed '" << printed << "'" << std::endl;
        }
        CHECK(equal(first, second));
    } catch (const std::exception& ex) {
        std::cout << "ROUND TRIP EXCEPTION for '" << input << "': " << ex.what() << std::endl;
        throw;
    }
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
    CHECK_EQ(to_full_form(parse_text("Sin[x] + Sqrt[4]")),
             std::string("Plus(2, Sin(x))"));
    CHECK_EQ(to_full_form(parse_text("Integrate[x^2, x, 0, 1]")),
             std::string("Integrate(Power(x, 2), List(x, 0, 1))"));
    CHECK_EQ(to_full_form(parse_text("{{a, b}, {c, d}}")),
             std::string("List(List(a, b), List(c, d))"));
    CHECK_EQ(to_full_form(parse_text("Plus[1, Times[2, x]]")),
             std::string("Plus(1, Times(2, x))"));
    CHECK_EQ(to_full_form(parse_text("Limit[x^2, x -> 0]")),
             std::string("Limit(Power(x, 2), Rule(x, 0))"));
    CHECK_EQ(to_full_form(parse_text("x^2 == 1")),
             std::string("Equal(Power(x, 2), 1)"));
}

TEST_CASE("Text: Tier 1 operators (/. , . , &&, ||, !, ', patterns)") {
    CHECK_EQ(to_full_form(parse_text("x^2 + y /. x -> 3")),
             std::string("ReplaceAll(Plus(Power(x, 2), y), Rule(x, 3))"));
    round_trip("x^2 + y /. x -> 3");

    CHECK_EQ(to_full_form(parse_text("A . B")),
             std::string("Dot(A, B)"));
    round_trip("A . B");

    CHECK_EQ(to_full_form(parse_text("x > 0 && y < 0")),
             std::string("And(Greater(x, 0), Less(y, 0))"));
    CHECK_EQ(to_full_form(parse_text("p || !q")),
             std::string("Or(p, Not(q))"));
    round_trip("x > 0 && y < 0");
    round_trip("p || !q");

    CHECK_EQ(to_full_form(parse_text("f'[x]")),
             std::string("Derivative(1, f)(x)"));
    CHECK_EQ(to_full_form(parse_text("y''[t]")),
             std::string("Derivative(2, y)(t)"));
    round_trip("f'[x]");
    round_trip("y''[t]");

    CHECK_EQ(to_full_form(parse_text("x_")),
             std::string("Pattern(x, Blank())"));
    CHECK_EQ(to_full_form(parse_text("x_Integer")),
             std::string("Pattern(x, Blank(Integer))"));
    CHECK_EQ(to_full_form(parse_text("x__")),
             std::string("Pattern(x, BlankSequence())"));
    CHECK_EQ(to_full_form(parse_text("x___")),
             std::string("Pattern(x, BlankNullSequence())"));
    CHECK_EQ(to_full_form(parse_text("_")),
             std::string("Blank()"));
    round_trip("f[x_] := x^2");
    round_trip("g[x_Integer] := x + 1");

    CHECK_EQ(to_full_form(parse_text("n!")),
             std::string("Factorial(n)"));
    round_trip("n!");
}

TEST_CASE("Text: stable print and parse") {
    round_trip("x^2 + 2*x + 1");
    round_trip("Integrate[Sin[x], x, 0, pi]");
    round_trip("{{a, 1/2}, {c, d}}");
    round_trip("f[x, y] + x^(1/2)");
    round_trip("(-8)^(1/3)");
    round_trip("`e` + e");
    round_trip("a b + c");
    round_trip("DSolve[y[x] == 0, y[x], x]");
    round_trip("Sin[x] + `Sin`");
    ExprPtr applied = make_normal(make_normal("f", {make_symbol("x")}), {make_symbol("y")});
    CHECK(equal(applied, parse_text(to_text(applied))));
    ExprPtr lower_head = make_normal("Sin", {make_symbol("x")});
    CHECK(equal(lower_head, parse_text(to_text(lower_head))));
}

TEST_CASE("Text: rejects malformed input") {
    CHECK_THROWS(parse_text(""));
    CHECK_THROWS(parse_text("x +"));
    CHECK_THROWS(parse_text("Sin[x"));
    CHECK_THROWS(parse_text("1 2"));
    CHECK_THROWS(parse_text("{a, b]"));
    CHECK_THROWS(parse_text("1..2"));
}

TEST_CASE("Text: readable output remains parseable") {
    CHECK_EQ(to_text(parse_text("x-y")), std::string("x - y"));
    CHECK_EQ(to_text(parse_text("-x^2")), std::string("-x^2"));
    CHECK_EQ(to_text(parse_text("Sin[x]")), std::string("Sin[x]"));
    CHECK_EQ(to_text(parse_text("x==2")), std::string("x == 2"));
    CHECK_EQ(to_text(parse_text("x->2")), std::string("x -> 2"));
    round_trip("x-y");
    round_trip("-2*x^2 + Sin[x]");
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
    round_trip("f[x] := x^2");
    CHECK_THROWS(parse_text("1."));
}

TEST_CASE("Text: space before parenthesis means multiplication") {
    CHECK_EQ(to_full_form(parse_text("x (y+1)")),
             std::string("Times(x, Plus(1, y))"));
    CHECK_EQ(to_full_form(parse_text("x(y+1)")),
             std::string("Times(x, Plus(1, y))"));
}

TEST_CASE("Text: nesting limit throws before stack exhaustion") {
    const std::string parens = std::string(400, '(') + "x" + std::string(400, ')');
    CHECK_THROWS(parse_text(parens));
    CHECK_THROWS(parse_text(std::string(400, '-') + "x"));
    ExprPtr deep = make_symbol("x");
    for (int i = 0; i < 300; ++i) deep = make_normal("f", {deep});
    CHECK_THROWS(to_text(deep));
}
