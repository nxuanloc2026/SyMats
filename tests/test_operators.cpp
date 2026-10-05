// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Tier 1 operators (AGENTS.md "Operator set"): logic, Dot, ReplaceAll.
// Built as trees here; the text syntax (&&, ||, !, ., /.) is parsed by convert/ (T-024).
#include "symats/eval.h"
#include "symats/pattern.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
ExprPtr call(std::string_view h, ExprList a) { return make_normal(h, std::move(a)); }
std::string run(Context& c, const ExprPtr& e) { return to_full_form(evaluate(e, c)); }
const ExprPtr T = make_symbol("True");
const ExprPtr Fa = make_symbol("False");
}  // namespace

TEST_CASE("Operators: And / Or / Not") {
    Context c;
    CHECK_EQ(run(c, call("And", {T, T})), std::string("True"));
    CHECK_EQ(run(c, call("And", {T, Fa})), std::string("False"));
    CHECK_EQ(run(c, call("Or", {Fa, T})), std::string("True"));
    CHECK_EQ(run(c, call("Or", {Fa, Fa})), std::string("False"));
    CHECK_EQ(run(c, call("Not", {T})), std::string("False"));
    CHECK_EQ(run(c, call("Not", {call("Not", {P("p")})})), std::string("p"));
    // Comparisons inside: 1 < 2 && 3 > 4
    CHECK_EQ(run(c, call("And", {P("1 < 2"), P("3 > 4")})), std::string("False"));
    // Undecided parts stay symbolic; decided parts drop out.
    CHECK_EQ(run(c, call("And", {T, P("x > 0")})), std::string("Greater(x, 0)"));
    CHECK_EQ(run(c, call("Or", {Fa, P("p"), P("q")})), std::string("Or(p, q)"));
}

TEST_CASE("Operators: And/Or stop early (short-circuit)") {
    Context c;
    // The second argument would recurse forever if evaluated.
    run(c, call("SetDelayed", {P("loop"), P("loop + 1")}));  // evaluating `loop` would throw
    CHECK_THROWS(evaluate(P("loop"), c));
    CHECK_EQ(run(c, call("And", {Fa, P("loop")})), std::string("False"));
    CHECK_EQ(run(c, call("Or", {T, P("loop")})), std::string("True"));
}

TEST_CASE("Operators: Dot for vectors and matrices") {
    Context c;
    CHECK_EQ(run(c, call("Dot", {P("{1, 2, 3}"), P("{4, 5, 6}")})), std::string("32"));
    CHECK_EQ(run(c, call("Dot", {P("{a, b}"), P("{c, d}")})), std::string("Plus(Times(a, c), Times(b, d))"));
    CHECK_EQ(run(c, call("Dot", {P("[[1, 2], [3, 4]]"), P("{5, 6}")})), std::string("List(17, 39)"));
    CHECK_EQ(run(c, call("Dot", {P("{1, 1}"), P("[[1, 2], [3, 4]]")})), std::string("List(4, 6)"));
    CHECK_EQ(run(c, call("Dot", {P("[[1, 2], [3, 4]]"), P("[[0, 1], [1, 0]]")})),
             std::string("List(List(2, 1), List(4, 3))"));
    // Non-square: (2x3).(3x1)
    CHECK_EQ(run(c, call("Dot", {P("[[1, 0, 2], [0, 1, 0]]"), P("[[1], [2], [3]]")})),
             std::string("List(List(7), List(2))"));
    // Chained A.B.v
    CHECK_EQ(run(c, call("Dot", {P("[[1, 0], [0, 2]]"), P("[[1, 1], [0, 1]]"), P("{1, 1}")})),
             std::string("List(2, 2)"));
    // Symbolic operands stay unevaluated; bad shapes are an error.
    CHECK_EQ(run(c, call("Dot", {P("A"), P("B")})), std::string("Dot(A, B)"));
    CHECK_THROWS(evaluate(call("Dot", {P("{1, 2}"), P("{1, 2, 3}")}), c));
}

TEST_CASE("Operators: ReplaceAll (/.) leaves variables unassigned") {
    Context c;
    CHECK_EQ(run(c, call("ReplaceAll", {P("x^2 + y"), P("x -> 3")})), std::string("Plus(9, y)"));
    CHECK_EQ(run(c, P("x^2 + y /. x -> 3")), std::string("Plus(9, y)"));
    CHECK_EQ(run(c, P("x")), std::string("x"));  // x still has no value
}

TEST_CASE("Operators: Derivative and Pattern syntax representations") {
    Context c;
    // Derivative: y'(t) -> Derivative(1)(y)(t)
    ExprPtr deriv1 = call("Derivative", {make_integer(1)});
    ExprPtr y_prime = make_normal(deriv1, {make_symbol("y")});
    ExprPtr y_prime_t = make_normal(y_prime, {make_symbol("t")});
    CHECK_EQ(to_full_form(y_prime_t), "Derivative(1)(y)(t)");

    // Pattern structures: x_, x_Integer, x__, x___
    ExprPtr px = pat("x");
    ExprPtr px_int = pat("x", "Integer");
    ExprPtr px_seq = pat_seq("x");
    ExprPtr px_null_seq = pat_null_seq("x");

    CHECK_EQ(to_full_form(px), "Pattern(x, Blank())");
    CHECK_EQ(to_full_form(px_int), "Pattern(x, Blank(Integer))");
    CHECK_EQ(to_full_form(px_seq), "Pattern(x, BlankSequence())");
    CHECK_EQ(to_full_form(px_null_seq), "Pattern(x, BlankNullSequence())");
}

TEST_CASE("Operators: Calculus expected values representation (D, Expand, Integrate, Solve)") {
    Context c;
    // D(x^3, x) -> 3*x^2
    ExprPtr d_expr = call("D", {P("x^3"), make_symbol("x")});
    ExprPtr d_expected = P("3*x^2");
    CHECK_EQ(to_full_form(d_expr), "D(Power(x, 3), x)");
    CHECK_EQ(to_full_form(d_expected), "Times(3, Power(x, 2))");

    // Expand((x + 1)^2) -> x^2 + 2*x + 1
    ExprPtr exp_expr = call("Expand", {P("(x + 1)^2")});
    ExprPtr exp_expected = P("x^2 + 2*x + 1");
    CHECK_EQ(to_full_form(exp_expr), "Expand(Power(Plus(1, x), 2))");
    CHECK_EQ(to_full_form(exp_expected), "Plus(1, Times(2, x), Power(x, 2))");

    // Integrate(Sin(x), x) -> -1 * Cos(x)
    ExprPtr int_expr = call("Integrate", {call("Sin", {make_symbol("x")}), make_symbol("x")});
    ExprPtr int_expected = call("Times", {make_integer(-1), call("Cos", {make_symbol("x")})});
    CHECK_EQ(to_full_form(int_expr), "Integrate(Sin(x), x)");
    CHECK_EQ(to_full_form(int_expected), "Times(-1, Cos(x))");

    // Solve(x^2 - 4 == 0, x) -> List(Rule(x, -2), Rule(x, 2))
    ExprPtr solve_expr = call("Solve", {call("Equal", {P("x^2 - 4"), make_integer(0)}), make_symbol("x")});
    ExprPtr solve_expected = call("List", {call("Rule", {make_symbol("x"), make_integer(-2)}),
                                           call("Rule", {make_symbol("x"), make_integer(2)})});
    CHECK_EQ(to_full_form(solve_expr), "Solve(Equal(Plus(-4, Power(x, 2)), 0), x)");
    CHECK_EQ(to_full_form(solve_expected), "List(Rule(x, -2), Rule(x, 2))");
}

TEST_CASE("Operators: ReplaceAll with list of rules and nested subexpressions") {
    Context c;
    CHECK_EQ(run(c, call("ReplaceAll", {P("x + y + z"), P("{x -> 1, y -> 2, z -> 3}")})), std::string("6"));
    CHECK_EQ(run(c, call("ReplaceAll", {P("f(x, y)"), P("{x -> a + 1, y -> b}")})), std::string("f(Plus(1, a), b)"));
}

TEST_CASE("Operators: Relational operators evaluation") {
    Context c;
    CHECK_EQ(run(c, call("Equal", {P("5"), P("5")})), std::string("True"));
    CHECK_EQ(run(c, call("Equal", {P("5"), P("6")})), std::string("False"));
    CHECK_EQ(run(c, call("Unequal", {P("5"), P("6")})), std::string("True"));
    CHECK_EQ(run(c, call("LessEqual", {P("5"), P("5")})), std::string("True"));
    CHECK_EQ(run(c, call("GreaterEqual", {P("5"), P("6")})), std::string("False"));
}
