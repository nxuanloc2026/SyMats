// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "symats/backend.h"
#include "symats/evaluator.h"
#include "symats/expr.h"
#include "symats/pattern.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {

class MockBackend : public MathBackend {
public:
    ExprPtr integrate(const ExprPtr& expr) override {
        if (expr->size() == 2 && expr->arg(0)->is_symbol("x") && expr->arg(1)->is_symbol("x")) {
            return parse_text("1/2 * x^2");
        }
        return expr;
    }

    ExprPtr solve(const ExprPtr& expr) override {
        (void)expr;
        return parse_text("{x -> 2}");
    }
};

}  // namespace

TEST_CASE("Evaluator: Pattern Matching and Rule Substitution") {
    MatchBindings bindings;
    ExprPtr e1 = parse_text("f(2)");
    ExprPtr p1 = make_normal("f", {make_normal("Pattern", {make_symbol("x"), make_normal("Blank", {make_symbol("Integer")})})});
    CHECK(match_pattern(e1, p1, bindings));
    CHECK_EQ(bindings["x"]->integer().to_string(), "2");

    MatchBindings bindings2;
    ExprPtr e2 = parse_text("f(a)");
    CHECK(!match_pattern(e2, p1, bindings2));

    ExprPtr pattern_lhs = make_normal("f", {make_normal("Pattern", {make_symbol("x"), make_normal("Blank", {})})});
    ExprPtr pattern_rhs = parse_text("x^2");
    ExprPtr rule = make_normal("Rule", {pattern_lhs, pattern_rhs});

    ExprPtr expr = parse_text("f(3) + f(4)");
    ExprPtr replaced = replace_all(expr, rule);
    CHECK_EQ(to_full_form(replaced), std::string("Plus(Power(3, 2), Power(4, 2))"));

    // BlankSequence matching (x__)
    MatchBindings seq_bindings;
    ExprPtr seq_expr = parse_text("g(1, 2, 3)");
    ExprPtr seq_pattern = make_normal("g", {make_normal("Pattern", {make_symbol("xs"), make_normal("BlankSequence", {})})});
    CHECK(match_pattern(seq_expr, seq_pattern, seq_bindings));
    CHECK(seq_bindings.find("xs") != seq_bindings.end());
}

TEST_CASE("Evaluator: Session, Set (=), SetDelayed (:=), and Evaluation Order") {
    Session session;

    // Immediate assignment Set (a = 2 + 3)
    ExprPtr set_expr = parse_text("a = 2 + 3");
    ExprPtr set_res = session.eval(set_expr);
    CHECK_EQ(to_full_form(set_res), "5");

    // Symbol 'a' now evaluates to 5
    CHECK_EQ(to_full_form(session.eval(parse_text("a"))), "5");

    // Set b = 2
    session.eval(parse_text("b = 2"));

    // Delayed assignment SetDelayed (f(x_) := x + a)
    ExprPtr pattern_lhs = make_normal("f", {make_normal("Pattern", {make_symbol("x"), make_normal("Blank", {make_symbol("Integer")})})});
    ExprPtr pattern_rhs = parse_text("x + a");
    ExprPtr set_delayed_expr = make_normal("SetDelayed", {pattern_lhs, pattern_rhs});

    ExprPtr delayed_res = session.eval(set_delayed_expr);
    CHECK_EQ(to_full_form(delayed_res), "Null");

    // Evaluation order check: calling f(b) evaluates b -> 2 first, then matches f(x_Integer) -> 2 + 5 = 7
    ExprPtr call_eval_order = session.eval(parse_text("f(b)"));
    CHECK_EQ(to_full_form(call_eval_order), "7");

    // f(10) evaluates x + a with a=5 => 10 + 5 = 15
    ExprPtr call_res = session.eval(parse_text("f(10)"));
    CHECK_EQ(to_full_form(call_res), "15");
}

TEST_CASE("Evaluator: Recursion Depth Guard") {
    Session session;
    // Define cyclic rule x = x + 1
    session.eval(parse_text("x = x + 1"));
    CHECK_THROWS(session.eval(parse_text("x")));
}

TEST_CASE("Evaluator: MathBackend Dispatch") {
    auto mock = std::make_shared<MockBackend>();
    register_math_backend(mock);

    Session session;
    ExprPtr int_res = session.eval(parse_text("integrate(x, x)"));
    CHECK_EQ(to_full_form(int_res), "Times(1/2, Power(x, 2))");

    ExprPtr solve_res = session.eval(parse_text("solve(x == 2, x)"));
    CHECK_EQ(to_full_form(solve_res), "List(Rule(x, 2))");

    register_math_backend(nullptr);
}
