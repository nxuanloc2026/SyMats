// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Tier 1 operators (AGENTS.md "Operator set"): logic, Dot, ReplaceAll.
// Built as trees here; the text syntax (&&, ||, !, ., /.) is parsed by convert/ (T-024).
#include "symats/eval.h"
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
    CHECK_EQ(run(c, P("x")), std::string("x"));  // x still has no value
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
