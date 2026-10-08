// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "symats/evaluator.h"
#include "symats/expr.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("Mathics Semantics: HoldAll Attribute") {
    Session session;
    session.set_attribute("HoldTest", "HoldAll");

    // Assign x = 5
    session.eval(parse_text("x = 5"));

    // HoldTest(x) should keep x unevaluated inside HoldTest
    ExprPtr expr = parse_text("HoldTest(x)");
    ExprPtr result = session.eval(expr);

    CHECK_EQ(to_full_form(result), "HoldTest(x)");
}

TEST_CASE("Mathics Semantics: Listable Attribute") {
    Session session;
    session.set_attribute("f", "Listable");

    // f(x_) := x
    ExprPtr pattern_lhs = make_normal("f", {make_normal("Pattern", {make_symbol("x"), make_normal("Blank", {})})});
    ExprPtr pattern_rhs = parse_text("x");
    session.eval(make_normal("SetDelayed", {pattern_lhs, pattern_rhs}));

    // f({1, 2, 3}) threads over list to return {1, 2, 3}
    ExprPtr expr = parse_text("f({1, 2, 3})");
    ExprPtr result = session.eval(expr);

    CHECK_EQ(to_full_form(result), "List(1, 2, 3)");
}

TEST_CASE("Mathics Semantics: Rule Replacement and Scope") {
    Session session;

    // Rule replacement: (x + y) /. {x -> 1, y -> 2}
    ExprPtr expr = parse_text("x + y");
    ExprPtr rule1 = make_normal("Rule", {make_symbol("x"), make_integer(1)});
    ExprPtr rule2 = make_normal("Rule", {make_symbol("y"), make_integer(2)});

    ExprPtr substituted = replace_all(expr, {rule1, rule2});
    ExprPtr result = session.eval(substituted);

    CHECK_EQ(to_full_form(result), "3");
}
