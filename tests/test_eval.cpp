// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <memory>
#include <optional>

#include "symats/eval.h"
#include "symats/pattern.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
std::string F(const ExprPtr& e) { return to_full_form(e); }
ExprPtr call(std::string_view h, ExprList a) { return make_normal(h, std::move(a)); }
ExprPtr S(std::string n) { return make_symbol(std::move(n)); }
std::string run(Context& c, const ExprPtr& e) { return F(evaluate(e, c)); }
std::string run(Context& c, std::string_view text) { return F(evaluate(P(text), c)); }
}  // namespace

TEST_CASE("Eval: arithmetic and canonical form") {
    Context c;
    CHECK_EQ(run(c, "1 + 2*3"), std::string("7"));
    CHECK_EQ(run(c, call("Plus", {S("x"), S("x")})), std::string("Times(2, x)"));
    CHECK_EQ(run(c, "(x + 1) - (x + 1)"), std::string("0"));
    CHECK_EQ(run(c, "2^10"), std::string("1024"));
}

TEST_CASE("Eval: own values, Clear, immediate vs delayed") {
    Context c;
    CHECK_EQ(run(c, "x = 5"), std::string("5"));
    CHECK_EQ(run(c, "x + 1"), std::string("6"));
    CHECK_EQ(run(c, call("Clear", {S("x")})), std::string("Null"));
    CHECK_EQ(run(c, "x + 1"), std::string("Plus(1, x)"));

    run(c, "y = 2");
    run(c, "b = y");    // immediate: b is 2 now
    run(c, "a := y");   // delayed: a follows y
    run(c, "y = 3");
    CHECK_EQ(run(c, "b"), std::string("2"));
    CHECK_EQ(run(c, "a"), std::string("3"));
}

TEST_CASE("Eval: recursive definitions with patterns (factorial)") {
    Context c;
    // Pattern rule first, exact rule second: exact rules are still tried first.
    evaluate(call("SetDelayed", {call("fact", {pat("n", "Integer")}), P("n*fact(n - 1)")}), c);
    evaluate(call("Set", {call("fact", {make_integer(0)}), make_integer(1)}), c);
    CHECK_EQ(run(c, "fact(5)"), std::string("120"));
    CHECK_EQ(run(c, "fact(20)"), std::string("2432902008176640000"));
    CHECK_EQ(F(evaluate(P("fact(100)"), c)).size(), std::size_t(158));
    CHECK_EQ(run(c, "fact(x)"), std::string("fact(x)"));  // x is not an Integer
}

TEST_CASE("Eval: sequence patterns and redefinition") {
    Context c;
    evaluate(call("Set", {call("len", {}), make_integer(0)}), c);
    evaluate(call("SetDelayed", {call("len", {pat("h"), pat_null_seq("rest")}),
                                 call("Plus", {make_integer(1), call("len", {S("rest")})})}), c);
    CHECK_EQ(run(c, "len(a, b, c, d)"), std::string("4"));
    CHECK_EQ(run(c, "len()"), std::string("0"));

    evaluate(call("SetDelayed", {call("f", {pat("x")}), P("x^2")}), c);
    evaluate(call("SetDelayed", {call("f", {pat("x")}), P("x^3")}), c);  // replaces
    CHECK_EQ(run(c, "f(2)"), std::string("8"));

    // Pattern variables are not captured by global values.
    run(c, "x = 5");
    evaluate(call("SetDelayed", {call("sq", {pat("x")}), P("x^2")}), c);
    CHECK_EQ(run(c, "sq(3)"), std::string("9"));

    // Left-hand side arguments are evaluated: g(1 + 1) = 7 defines g(2).
    run(c, "g(1 + 1) = 7");
    CHECK_EQ(run(c, "g(2)"), std::string("7"));
}

TEST_CASE("Eval: Listable threading") {
    Context c;
    CHECK_EQ(run(c, "{1, 2, 3} + 10"), std::string("List(11, 12, 13)"));
    CHECK_EQ(run(c, "{1, 2} * {3, 4}"), std::string("List(3, 8)"));
    CHECK_EQ(run(c, "sin({x, y})"), std::string("List(Sin(x), Sin(y))"));
    CHECK_EQ(run(c, "[[1, 2], [3, 4]] * 2"), std::string("List(List(2, 4), List(6, 8))"));
    // Mismatched lengths: left alone, no crash.
    CHECK(evaluate(P("{1, 2} + {1, 2, 3}"), c)->has_head("Plus"));
}

TEST_CASE("Eval: Hold, Sequence, Substitute") {
    Context c;
    // Build 1 + 1 raw (the text parser would already simplify it to 2).
    ExprPtr raw_sum = call("Plus", {make_integer(1), make_integer(1)});
    CHECK_EQ(run(c, call("Hold", {raw_sum})), std::string("Hold(Plus(1, 1))"));
    CHECK_EQ(run(c, raw_sum), std::string("2"));
    CHECK_EQ(run(c, call("f", {call("Sequence", {make_integer(1), make_integer(2)}), make_integer(3)})),
             std::string("f(1, 2, 3)"));
    CHECK_EQ(run(c, "subs(x^2 + y, x -> 3)"), std::string("Plus(9, y)"));
    CHECK_EQ(run(c, "subs(x + y, {x -> 1, y -> 2})"), std::string("3"));
}

TEST_CASE("Eval: comparisons") {
    Context c;
    CHECK_EQ(run(c, "2 == 2"), std::string("True"));
    CHECK_EQ(run(c, "1 == 2"), std::string("False"));
    CHECK_EQ(run(c, "x == x"), std::string("True"));
    CHECK_EQ(run(c, "x == y"), std::string("Equal(x, y)"));
    CHECK_EQ(run(c, "1 < 2"), std::string("True"));
    CHECK_EQ(run(c, "1/2 < 1/3"), std::string("False"));
    CHECK_EQ(run(c, "3 != 3"), std::string("False"));
}

TEST_CASE("Eval: protected symbols refuse definitions") {
    Context c;
    CHECK_THROWS(evaluate(P("pi = 3"), c));
    CHECK_THROWS(evaluate(call("SetDelayed", {call("Sin", {pat("x")}), make_integer(0)}), c));
    CHECK_THROWS(evaluate(call("Set", {make_integer(1), make_integer(2)}), c));
}

TEST_CASE("Eval: runaway definitions hit limits, context stays usable") {
    Context c;
    // x = x + 1 stores x -> 1 + x; using x then recurses forever.
    CHECK_THROWS(evaluate(P("x = x + 1"), c));
    CHECK_THROWS(evaluate(P("x"), c));
    run(c, call("Clear", {S("x")}));
    CHECK_EQ(run(c, "x"), std::string("x"));
    run(c, "y = 1");
    CHECK_EQ(run(c, "y + 1"), std::string("2"));  // depth counter was restored

    evaluate(call("SetDelayed", {call("h", {pat("n")}), P("h(n + 1)")}), c);
    c.max_iterations = 2000;
    CHECK_THROWS(evaluate_top(P("h(1)"), c));

    // The rewrite budget is per outermost evaluation, not per session.
    c.max_iterations = 100000;
    for (int i = 0; i < 3000; ++i) evaluate(P("1 + 1"), c);
    CHECK_EQ(run(c, "y + 1"), std::string("2"));

    // A definition that rewrites to itself does not loop.
    evaluate(call("SetDelayed", {call("same", {pat("n")}), call("same", {S("n")})}), c);
    CHECK_EQ(run(c, "same(4)"), std::string("same(4)"));
}

namespace {
// A fake backend: knows only that ∫ x^2 dx = x^3/3.
class MockIntegrator : public MathBackend {
public:
    std::string name() const override { return "mock"; }
    bool supports(std::string_view head) const override { return head == "Integrate"; }
    std::optional<BackendResult> evaluate(const ExprPtr& e) override {
        ++calls;
        if (e->size() == 2 && equal(e->arg(0), parse_text("x^2")) && e->arg(1)->is_symbol("x"))
            return BackendResult{parse_text("x^3/3"), ResultStatus::Unverified, ""};
        return std::nullopt;
    }
    int calls = 0;
};

class ThrowingBackend : public MathBackend {
public:
    std::string name() const override { return "broken"; }
    bool supports(std::string_view) const override { return true; }
    std::optional<BackendResult> evaluate(const ExprPtr&) override { throw std::runtime_error("boom"); }
};
}  // namespace

TEST_CASE("Eval: dispatch to MathBackend with status") {
    Context c;
    auto mock = std::make_shared<MockIntegrator>();
    c.backends().add(std::make_shared<ThrowingBackend>());  // declines by throwing
    c.backends().add(mock);

    // Arguments are evaluated first: x*x becomes x^2 before the backend sees it.
    EvalResult r = evaluate_top(P("integrate(x*x, x)"), c);
    CHECK_EQ(F(r.value), std::string("Times(1/3, Power(x, 3))"));
    CHECK(r.status == ResultStatus::Unverified);
    CHECK_EQ(std::string(to_string(r.status)), std::string("unverified"));

    // Declined: stays unevaluated, status stays exact.
    EvalResult r2 = evaluate_top(P("integrate(sin(x)/x, x)"), c);
    CHECK(r2.value->has_head("Integrate"));
    CHECK(r2.status == ResultStatus::Exact);

    // Plain arithmetic never reaches a backend.
    int before = mock->calls;
    CHECK_EQ(F(evaluate_top(P("1 + 1"), c).value), std::string("2"));
    CHECK_EQ(mock->calls, before);
}
