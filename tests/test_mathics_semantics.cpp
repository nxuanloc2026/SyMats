// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Evaluator semantics tests based on Mathics3 doc-tests & evaluator behavior.
// Reference system: Mathics3 (GPL, reference/oracle only).
#include "symats/eval.h"
#include "symats/pattern.h"
#include "symats/session.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
ExprPtr call(std::string_view h, ExprList a) { return make_normal(h, std::move(a)); }
std::string eval_str(Context& c, std::string_view expr_text) {
    return to_full_form(evaluate(P(expr_text), c));
}
}  // namespace

TEST_CASE("Mathics3 Semantics: Set and SetDelayed evaluation order") {
    Context c;

    // Immediate assignment: Set(x, 2 + 3) evaluates rhs immediately
    evaluate(call("Set", {make_symbol("x"), P("2 + 3")}), c);
    CHECK_EQ(eval_str(c, "x"), "5");

    // Delayed assignment: SetDelayed(f(a_), a^2) evaluates rhs at call site
    evaluate(call("SetDelayed", {call("f", {pat("a")}), P("a^2")}), c);
    CHECK_EQ(eval_str(c, "f(4)"), "16");

    // Overwriting definition
    evaluate(call("Set", {make_symbol("x"), P("10")}), c);
    CHECK_EQ(eval_str(c, "x"), "10");
}

TEST_CASE("Mathics3 Semantics: Hold, Sequence, and HoldAll attributes") {
    Context c;

    // Built-in Hold with HoldAll attribute prevents evaluation of arguments
    ExprPtr held = call("Hold", {call("Plus", {make_integer(1), make_integer(1)})});
    CHECK_EQ(to_full_form(evaluate(held, c)), "Hold(Plus(1, 1))");

    // Sequence splicing in function arguments
    ExprPtr seq_expr = call("List", {make_integer(1), call("Sequence", {make_integer(2), make_integer(3)}), make_integer(4)});
    CHECK_EQ(to_full_form(evaluate(seq_expr, c)), "List(1, 2, 3, 4)");
}

TEST_CASE("Mathics3 Semantics: Listable threading behavior") {
    Context c;

    // Unary Listable function threaded over lists: f({a, b}) -> List(f(a), f(b))
    c.set_attributes("f", attr::Listable);
    ExprPtr threaded = evaluate(P("f({a, b})"), c);
    CHECK_EQ(to_full_form(threaded), "List(f(a), f(b))");

    // Binary addition threading over equal-length lists
    ExprPtr list_plus = evaluate(P("{1, 2} + {3, 4}"), c);
    CHECK_EQ(to_full_form(list_plus), "List(4, 6)");
}

TEST_CASE("Mathics3 Semantics: CompoundExpression trailing value") {
    Session s;

    // CompoundExpression returns value of last expression
    auto r1 = s.run(call("CompoundExpression", {P("a = 2"), P("b = 3"), P("a + b")}));
    CHECK(r1.ok());
    CHECK_EQ(to_full_form(r1.output), "5");

    // CompoundExpression with trailing Null is suppressed
    auto r2 = s.run(call("CompoundExpression", {P("c = 4"), make_symbol("Null")}));
    CHECK(r2.ok());
    CHECK(!r2.visible());
}

TEST_CASE("Mathics3 Semantics: ReplaceAll rule matching and evaluation") {
    Context c;

    // Single rule replacement
    ExprPtr rep1 = evaluate(call("ReplaceAll", {P("x^2 + x + 1"), call("Rule", {make_symbol("x"), make_integer(3)})}), c);
    CHECK_EQ(to_full_form(rep1), "13");

    // List of rules replacement
    ExprPtr rep2 = evaluate(call("ReplaceAll", {P("a*x + b"), call("List", {call("Rule", {make_symbol("a"), make_integer(2)}), call("Rule", {make_symbol("b"), make_integer(5)})})}), c);
    CHECK_EQ(to_full_form(rep2), "Plus(5, Times(2, x))");
}
