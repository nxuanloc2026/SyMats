// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/latex.h"
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
    CHECK_EQ(to_full_form(parse_text("2x + 3*x")), std::string("Times[5, x]"));
    CHECK_EQ(to_full_form(parse_text("-x^2")), std::string("Times[-1, Power[x, 2]]"));
    CHECK_EQ(to_full_form(parse_text("x^-2")), std::string("Power[x, -2]"));
    CHECK_EQ(to_full_form(parse_text("1/2 + 1/3")), std::string("5/6"));
    CHECK_EQ(to_full_form(parse_text("2^3^2")), std::string("512"));
    CHECK_EQ(to_full_form(parse_text("a b")), std::string("Times[a, b]"));
    CHECK_EQ(to_full_form(parse_text("123456789012345678901234567890")),
             std::string("123456789012345678901234567890"));
}

TEST_CASE("Text: calls, matrices, and internal form") {
    CHECK_EQ(to_full_form(parse_text("Sin[x] + Sqrt[4]")),
             std::string("Plus[2, Sin[x]]"));
    CHECK_EQ(to_full_form(parse_text("Integrate[x^2, {x, 0, 1}]")),
             std::string("Integrate[Power[x, 2], List[x, 0, 1]]"));
    CHECK_EQ(to_full_form(parse_text("{{a,b},{c,d}}")),
             std::string("List[List[a, b], List[c, d]]"));
    CHECK_EQ(to_full_form(parse_text("Plus[1, Times[2, x]]")),
             std::string("Plus[1, Times[2, x]]"));
    CHECK_EQ(to_full_form(parse_text("Limit[x^2, x -> 0]")),
             std::string("Limit[Power[x, 2], Rule[x, 0]]"));
    CHECK_EQ(to_full_form(parse_text("x^2 == 1")),
             std::string("Equal[Power[x, 2], 1]"));
}

TEST_CASE("Text: stable print and parse") {
    round_trip("x^2 + 2*x + 1");
    round_trip("Integrate[Sin[x], {x, 0, Pi}]");
    round_trip("{{a, 1/2}, {c, d}}");
    round_trip("f[x, y] + x^(1/2)");
    round_trip("(-8)^(1/3)");
    round_trip("`e` + e");
    round_trip("a b + c");
    round_trip("DSolve[y[x] == 0, y[x], x]");
    round_trip("Sin[x] + `sin`");
    ExprPtr applied = make_normal(make_normal("f", {make_symbol("x")}), {make_symbol("y")});
    CHECK(equal(applied, parse_text(to_text(applied))));
    ExprPtr lower_head = make_normal("sin", {make_symbol("x")});
    CHECK(equal(lower_head, parse_text(to_text(lower_head))));
}

TEST_CASE("Text: rejects malformed input") {
    CHECK_THROWS(parse_text(""));
    CHECK_THROWS(parse_text("x +"));
    CHECK_THROWS(parse_text("Sin[x"));
    CHECK_THROWS(parse_text("1 2"));
    CHECK_THROWS(parse_text("[a, b}"));
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
    CHECK_EQ(to_full_form(parse_text("x = 2")), std::string("Set[x, 2]"));
    CHECK_EQ(to_full_form(parse_text("x := 2")), std::string("SetDelayed[x, 2]"));
    CHECK_EQ(to_full_form(parse_text("x == 2")), std::string("Equal[x, 2]"));
    round_trip("1.5*x = 3.75");
    round_trip("f[x] := x^2");
    CHECK_THROWS(parse_text("1."));
}

TEST_CASE("Text: parentheses group multiplication; brackets call functions") {
    CHECK_EQ(to_full_form(parse_text("x (y+1)")),
             std::string("Times[x, Plus[1, y]]"));
    CHECK_EQ(to_full_form(parse_text("x[y+1]")),
             std::string("x[Plus[1, y]]"));
    CHECK_EQ(to_full_form(parse_text("x(y+1)")),
             std::string("Times[x, Plus[1, y]]"));
}

TEST_CASE("Text: Tier 1 square bracket syntax is case-sensitive") {
    CHECK_EQ(to_full_form(parse_text("Sin[x]")), std::string("Sin[x]"));
    CHECK_EQ(to_full_form(parse_text("sin[x]")), std::string("sin[x]"));
    CHECK_EQ(to_text(parse_text("f[x_] := Sin[x]")), std::string("f[x_] := Sin[x]"));
    CHECK_EQ(to_full_form(parse_text("f[x_Integer]")),
             std::string("f[Pattern[x, Blank[Integer]]]"));
    CHECK_EQ(to_full_form(parse_text("{{a, b}, {c, d}}")),
             std::string("List[List[a, b], List[c, d]]"));
    CHECK_EQ(to_full_form(parse_text("Rational[1, 3]")), std::string("1/3"));
    round_trip("f[x][y]");
}

TEST_CASE("Text: nesting limit throws before stack exhaustion") {
    const std::string parens = std::string(400, '(') + "x" + std::string(400, ')');
    CHECK_THROWS(parse_text(parens));
    CHECK_THROWS(parse_text(std::string(400, '-') + "x"));
    ExprPtr deep = make_symbol("x");
    for (int i = 0; i < 300; ++i) deep = make_normal("f", {deep});
    CHECK_THROWS(to_text(deep));
}

TEST_CASE("Text: parse_cell handles multi-statement, comments, continuations, and percents") {
    std::string cell_code =
        "(* Define variables *)\n"
        "a = 5;\n"
        "b = 10;\n"
        "a +\n"
        "  b;\n"
        "% + %%\n"
        "%2 + 1";

    auto stmts = parse_cell(cell_code);
    CHECK_EQ(stmts.size(), std::size_t(5));

    // Stmt 1: a = 5 (suppressed)
    CHECK(stmts[0].suppressed);
    CHECK_EQ(to_full_form(stmts[0].expr), std::string("Set[a, 5]"));

    // Stmt 2: b = 10 (suppressed)
    CHECK(stmts[1].suppressed);
    CHECK_EQ(to_full_form(stmts[1].expr), std::string("Set[b, 10]"));

    // Stmt 3: a + b (continued across lines, suppressed by trailing ;)
    CHECK(stmts[2].suppressed);
    CHECK_EQ(to_full_form(stmts[2].expr), std::string("Plus[a, b]"));

    // Stmt 4: Out[-1] + Out[-2] (not suppressed)
    CHECK(!stmts[3].suppressed);
    CHECK_EQ(to_full_form(stmts[3].expr), std::string("Plus[Out[-2], Out[-1]]"));

    // Stmt 5: Out[2] + 1 (not suppressed)
    CHECK(!stmts[4].suppressed);
    CHECK_EQ(to_full_form(stmts[4].expr), std::string("Plus[1, Out[2]]"));
}

TEST_CASE("LaTeX: formatting expressions into LaTeX math strings") {
    CHECK_EQ(to_latex(parse_text("1/3")), std::string("\\frac{1}{3}"));
    CHECK_EQ(to_latex(parse_text("-1/2")), std::string("-\\frac{1}{2}"));
    CHECK_EQ(to_latex(parse_text("alpha + beta")), std::string("\\alpha + \\beta"));
    CHECK_EQ(to_latex(parse_text("Sin[x]")), std::string("\\sin\\left(x\\right)"));
    CHECK_EQ(to_latex(parse_text("Sqrt[x]")), std::string("\\sqrt{x}"));
    CHECK_EQ(to_latex(parse_text("{{a, b}, {c, d}}")),
             std::string("\\begin{pmatrix}a & b \\\\ c & d\\end{pmatrix}"));
    CHECK_EQ(to_latex(parse_text("Integrate[x^2, x]")),
             std::string("\\int x^{2} \\, dx"));
}

TEST_CASE("Text: Tier 1 operators parsing and printing") {
    // ReplaceAll (/.)
    CHECK_EQ(to_full_form(parse_text("x^2 + y /. x -> 3")), std::string("ReplaceAll[Plus[Power[x, 2], y], Rule[x, 3]]"));
    CHECK_EQ(to_text(parse_text("x^2 /. x -> 3")), std::string("x^2 /. x -> 3"));
    round_trip("x^2 /. x -> 3");

    // Logic: &&, ||, !
    CHECK_EQ(to_full_form(parse_text("a && b || c")), std::string("Or[And[a, b], c]"));
    CHECK_EQ(to_full_form(parse_text("!a && !b")), std::string("And[Not[a], Not[b]]"));
    CHECK_EQ(to_text(parse_text("a && b || c")), std::string("a && b || c"));
    round_trip("a && b || c");
    round_trip("!a && !b");

    // Dot (.)
    CHECK_EQ(to_full_form(parse_text("A . B . C")), std::string("Dot[A, B, C]"));
    CHECK_EQ(to_text(parse_text("A . B")), std::string("A . B"));
    round_trip("A . B");

    // Factorial (!)
    CHECK_EQ(to_full_form(parse_text("n!")), std::string("Factorial[n]"));
    CHECK_EQ(to_text(parse_text("n!")), std::string("n!"));
    round_trip("n!");

    // Derivatives (primes)
    CHECK_EQ(to_full_form(parse_text("y'[t]")), std::string("Derivative[1][y][t]"));
    CHECK_EQ(to_full_form(parse_text("f''[x]")), std::string("Derivative[2][f][x]"));
    CHECK_EQ(to_text(parse_text("y'[t]")), std::string("y'[t]"));
    CHECK_EQ(to_text(parse_text("f''[x]")), std::string("f''[x]"));
    round_trip("y'[t]");
    round_trip("f''[x]");
}
