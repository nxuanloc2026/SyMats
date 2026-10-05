// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Evaluator semantics audit vs Mathics3 reference behavior (GPL, reference only).
// Tests core evaluator attributes, Set/SetDelayed evaluation timing, Listable threading,
// Hold semantics, and rule substitution scoping.

#include "symats/eval.h"
#include "symats/pattern.h"
#include "symats/session.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
std::string F(const ExprPtr& e) { return e ? to_full_form(e) : std::string("<null>"); }
StatementResult run_code(Session& s, std::string_view code) { return s.run(P(code)); }
}  // namespace

TEST_CASE("Mathics3 semantics: Set (=) vs SetDelayed (:=) evaluation order") {
    Session s;
    // Set evaluates RHS immediately
    run_code(s, "val = 10");
    run_code(s, "a = val + 5");     // immediate: a stored as 15
    run_code(s, "b := val + 5");    // delayed: b holds val + 5
    run_code(s, "val = 20");

    CHECK_EQ(F(run_code(s, "a").output), std::string("15"));
    CHECK_EQ(F(run_code(s, "b").output), std::string("25"));

    // LHS argument evaluation on Set
    run_code(s, "idx = 2");
    run_code(s, "f[idx] = 100");    // defines f(2) = 100
    run_code(s, "idx = 3");
    CHECK_EQ(F(run_code(s, "f[2]").output), std::string("100"));
    CHECK_EQ(F(run_code(s, "f[3]").output), std::string("f(3)"));
}

TEST_CASE("Mathics3 semantics: Listable attribute threading") {
    Session s;
    // Scalar + List
    CHECK_EQ(F(run_code(s, "5 + {1, 2, 3}").output), std::string("List(6, 7, 8)"));
    // List + List (same dimension)
    CHECK_EQ(F(run_code(s, "{1, 2} + {10, 20}").output), std::string("List(11, 22)"));
    // List * List
    CHECK_EQ(F(run_code(s, "{2, 3} * {4, 5}").output), std::string("List(8, 15)"));
    // Listable function application over List
    CHECK_EQ(F(run_code(s, "Sin[{a, b}]").output), std::string("List(Sin(a), Sin(b))"));
    // Nested lists (matrices)
    CHECK_EQ(F(run_code(s, "{{1, 2}, {3, 4}} * 3").output), std::string("List(List(3, 6), List(9, 12))"));
}

TEST_CASE("Mathics3 semantics: Hold and Sequence evaluation control") {
    Session s;
    // Hold prevents argument evaluation when constructed directly with an unevaluated expression tree
    ExprPtr unsimplified_sum = make_normal("Plus", {make_integer(1), make_integer(1)});
    ExprPtr hold_expr = make_normal("Hold", {unsimplified_sum});
    CHECK_EQ(F(s.run(hold_expr).output), std::string("Hold(Plus(1, 1))"));

    // Set inside Hold is not executed
    ExprPtr set_expr = make_normal("Set", {make_symbol("x"), make_integer(5)});
    CHECK_EQ(F(s.run(make_normal("Hold", {set_expr})).output), std::string("Hold(Set(x, 5))"));
    // Confirm x was not assigned
    CHECK_EQ(F(run_code(s, "x").output), std::string("x"));

    // Sequence flattens into outer function arguments
    CHECK_EQ(F(run_code(s, "f[1, Sequence[2, 3], 4]").output), std::string("f(1, 2, 3, 4)"));
}

TEST_CASE("Mathics3 semantics: Protected built-in symbol protection") {
    Session s;
    // Assigning to protected constants or heads throws or fails
    CHECK(!run_code(s, "pi = 3.14").ok());
    CHECK(!run_code(s, "E = 2.718").ok());
    CHECK(!run_code(s, "I = 1").ok());
    CHECK(!run_code(s, "Sin = 5").ok());

    // User symbols can be assigned and cleared
    CHECK(run_code(s, "myVar = 42").ok());
    CHECK_EQ(F(run_code(s, "myVar").output), std::string("42"));
    run_code(s, "Clear[myVar]");
    CHECK_EQ(F(run_code(s, "myVar").output), std::string("myVar"));
}

TEST_CASE("Mathics3 semantics: Rule substitution and pattern scoping") {
    Session s;
    // Single rule substitution
    CHECK_EQ(F(run_code(s, "subs[x^2 + x, x -> 3]").output), std::string("12"));
    // Rule list substitution
    CHECK_EQ(F(run_code(s, "subs[x + y, {x -> 1, y -> 2}]").output), std::string("3"));

    // Pattern scope does not clash with outer global values
    run_code(s, "x = 99");
    s.run(make_normal("SetDelayed", {make_normal("square", {pat("x")}), P("x^2")}));
    CHECK_EQ(F(run_code(s, "square[4]").output), std::string("16"));
}

TEST_CASE("Mathics3 semantics: Relational and logical evaluation") {
    Session s;
    CHECK_EQ(F(run_code(s, "10 == 10").output), std::string("True"));
    CHECK_EQ(F(run_code(s, "10 == 20").output), std::string("False"));
    CHECK_EQ(F(run_code(s, "5 < 10").output), std::string("True"));
    CHECK_EQ(F(run_code(s, "10 <= 10").output), std::string("True"));
    CHECK_EQ(F(run_code(s, "5 > 10").output), std::string("False"));
    CHECK_EQ(F(run_code(s, "10 != 10").output), std::string("False"));
    CHECK_EQ(F(run_code(s, "10 != 20").output), std::string("True"));
}
