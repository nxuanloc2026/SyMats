// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/pattern.h"
#include "symats/session.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ExprPtr P(std::string_view s) { return parse_text(s); }
std::string F(const ExprPtr& e) { return e ? to_full_form(e) : std::string("<null>"); }
ExprPtr call(std::string_view h, ExprList a) { return make_normal(h, std::move(a)); }
}  // namespace

TEST_CASE("Session: definitions persist across cells") {
    Session s;
    // Cell 1
    auto c1 = s.run_cell({{P("a = 5"), true}, {P("b = a^2"), false}});
    CHECK_EQ(c1.size(), std::size_t(2));
    CHECK_EQ(c1[0].line, std::size_t(1));
    CHECK(!c1[0].visible());  // suppressed with ';'
    CHECK_EQ(F(c1[1].output), std::string("25"));
    CHECK(c1[1].visible());
    // Cell 2, later: still sees a and b
    auto c2 = s.run_cell({{P("a + b"), false}});
    CHECK_EQ(c2[0].line, std::size_t(3));
    CHECK_EQ(F(c2[0].output), std::string("30"));
}

TEST_CASE("Session: function definitions across cells; Null is not shown") {
    Session s;
    auto d = s.run(call("SetDelayed", {call("f", {pat("x")}), P("x^2 + 1")}));
    CHECK(d.ok());
    CHECK(!d.visible());  // SetDelayed returns Null
    CHECK_EQ(F(s.run(P("f(3)")).output), std::string("10"));
    CHECK_EQ(F(s.run(P("f(t)")).output), std::string("Plus(1, Power(t, 2))"));
}

TEST_CASE("Session: errors are per statement and the kernel survives") {
    Session s;
    auto r = s.run_cell({{P("x = x + 1"), false}, {P("y = 2"), false}, {P("y + 1"), false}});
    CHECK(!r[0].ok());
    CHECK(!r[0].error.empty());
    CHECK(!r[0].visible());
    CHECK(r[1].ok());
    CHECK_EQ(F(r[2].output), std::string("3"));
}

TEST_CASE("Session: Out history (% and Out(n))") {
    Session s;
    s.run(P("2 + 3"));                                            // Out[1] = 5
    s.run(P("10"));                                               // Out[2] = 10
    CHECK_EQ(F(s.run(call("Out", {})).output), std::string("10"));  // %  -> line 2
    CHECK_EQ(F(s.run(call("Out", {make_integer(1)})).output), std::string("5"));
    // % + %% : line 5 sees line 4 (=5) and line 3 (=10)
    CHECK_EQ(F(s.run(call("Plus", {call("Out", {make_integer(-1)}), call("Out", {make_integer(-2)})})).output),
             std::string("15"));
    CHECK(!s.run(call("Out", {make_integer(99)})).ok());
    CHECK_EQ(F(s.out(1)), std::string("5"));
    CHECK(s.out(0) == nullptr);
}

TEST_CASE("Session: CompoundExpression") {
    Session s;
    ExprPtr seq = call("CompoundExpression", {P("p = 2"), P("q = p + 1"), P("p*q")});
    CHECK_EQ(F(s.run(seq).output), std::string("6"));
    ExprPtr trailing = call("CompoundExpression", {P("r = 1"), make_symbol("Null")});
    auto t = s.run(trailing);
    CHECK(t.ok());
    CHECK(!t.visible());
}

TEST_CASE("Session: workspace listing and restart") {
    Session s;
    s.run(P("alpha = 1"));
    s.run(call("SetDelayed", {call("g", {pat("x")}), P("x")}));
    s.run(P("sin(0) + 1"));  // defines nothing
    auto names = s.user_symbols();
    CHECK_EQ(names.size(), std::size_t(2));
    CHECK_EQ(names[0], std::string("alpha"));
    CHECK_EQ(names[1], std::string("g"));

    s.restart();
    CHECK(s.user_symbols().empty());
    CHECK_EQ(s.next_line(), std::size_t(1));
    CHECK_EQ(F(s.run(P("alpha")).output), std::string("alpha"));
    CHECK_EQ(F(s.run(call("Out", {})).output), std::string("alpha"));  // Out works after restart
}

TEST_CASE("Session: multi-cell line numbering, history offset, and symbol clearing") {
    Session s;
    // Cell 1 (3 statements)
    auto c1 = s.run_cell({{P("x = 10"), false}, {P("y = 20"), false}, {P("x + y"), false}});
    CHECK_EQ(c1.size(), std::size_t(3));
    CHECK_EQ(c1[0].line, std::size_t(1));
    CHECK_EQ(c1[1].line, std::size_t(2));
    CHECK_EQ(c1[2].line, std::size_t(3));
    CHECK_EQ(s.next_line(), std::size_t(4));

    // % should yield line 3 (30)
    auto c2 = s.run(call("Out", {}));
    CHECK_EQ(c2.line, std::size_t(4));
    CHECK_EQ(F(c2.output), std::string("30"));

    // Clear symbol 'x'
    s.run(call("Clear", {make_symbol("x")})); // Line 5
    auto c3 = s.run(P("x + y"));              // Line 6 -> Plus(x, 20)
    CHECK_EQ(F(c3.output), std::string("Plus(20, x)"));

    // Check workspace active symbols: x was cleared, y remains
    auto symbols = s.user_symbols();
    CHECK_EQ(symbols.size(), std::size_t(1));
    CHECK_EQ(symbols[0], std::string("y"));
}
