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

TEST_CASE("Text: nesting limit throws before stack exhaustion") {
    const std::string parens = std::string(400, '(') + "x" + std::string(400, ')');
    CHECK_THROWS(parse_text(parens));
    CHECK_THROWS(parse_text(std::string(400, '-') + "x"));
    ExprPtr deep = make_symbol("x");
    for (int i = 0; i < 300; ++i) deep = make_normal("f", {deep});
    CHECK_THROWS(to_text(deep));
}

TEST_CASE("Text: history shortcuts") {
    CHECK_EQ(to_full_form(parse_text("%")), std::string("Out()"));
    CHECK_EQ(to_full_form(parse_text("%%")), std::string("Out(-2)"));
    CHECK_EQ(to_full_form(parse_text("%5")), std::string("Out(5)"));
    CHECK_EQ(to_full_form(parse_text("%-3")), std::string("Out(-3)"));
    CHECK_EQ(to_full_form(parse_text("% + %1")), std::string("Plus(Out(), Out(1))"));
}

TEST_CASE("Text: compound expressions") {
    CHECK_EQ(to_full_form(parse_text("a = 1; b = 2")),
             std::string("CompoundExpression(Set(a, 1), Set(b, 2))"));
    CHECK_EQ(to_full_form(parse_text("(x = 1; y = 2;)")),
             std::string("CompoundExpression(Set(x, 1), Set(y, 2), Null)"));
}

TEST_CASE("Text: parse_cell multi-statement splitting") {
    auto stmts1 = parse_cell("x = 5;\ny = 10");
    CHECK_EQ(stmts1.size(), std::size_t(2));
    CHECK_EQ(to_full_form(stmts1[0].expr), std::string("Set(x, 5)"));
    CHECK(stmts1[0].suppressed);
    CHECK_EQ(to_full_form(stmts1[1].expr), std::string("Set(y, 10)"));
    CHECK(!stmts1[1].suppressed);

    auto stmts2 = parse_cell("a = 1 +\n    2;\nb = a^2");
    CHECK_EQ(stmts2.size(), std::size_t(2));
    CHECK_EQ(to_full_form(stmts2[0].expr), std::string("Set(a, 3)"));
    CHECK(stmts2[0].suppressed);
    CHECK_EQ(to_full_form(stmts2[1].expr), std::string("Set(b, Power(a, 2))"));
    CHECK(!stmts2[1].suppressed);

    auto stmts3 = parse_cell("(* comment *)\na = 1; (* set a *)");
    CHECK_EQ(stmts3.size(), std::size_t(1));
    CHECK_EQ(to_full_form(stmts3[0].expr), std::string("Set(a, 1)"));
    CHECK(stmts3[0].suppressed);
}
