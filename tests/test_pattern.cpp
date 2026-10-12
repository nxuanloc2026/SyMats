// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/pattern.h"
#include "test.h"

using namespace symats;

namespace {
const Ex x = sym("x");
const Ex y = sym("y");
Ex f(ExprList a) { return make_normal("f", std::move(a)); }
Ex g(ExprList a) { return make_normal("g", std::move(a)); }
std::string F(const ExprPtr& e) { return to_full_form(e); }
}  // namespace

TEST_CASE("Pattern: blanks and named patterns") {
    Bindings b;
    CHECK(match(blank(), x, b));
    CHECK(b.empty());
    CHECK(match(pat("a"), f({x}), b));
    CHECK_EQ(F(b.at("a")), std::string("f[x]"));

    Bindings c;
    CHECK(match(f({pat("a"), pat("b")}), f({x, Ex(2)}), c));
    CHECK_EQ(F(c.at("a")), std::string("x"));
    CHECK_EQ(F(c.at("b")), std::string("2"));

    Bindings d;
    CHECK(!match(f({pat("a")}), g({x}), d));     // different head
    CHECK(!match(f({pat("a")}), f({x, y}), d));  // wrong arity
    CHECK(d.empty());                             // unchanged on failure
}

TEST_CASE("Pattern: head restrictions") {
    Bindings b;
    CHECK(match(pat("n", "Integer"), Ex(5), b));
    CHECK(!match(pat("m", "Integer"), x, b));
    CHECK(match(pat("r", "Rational"), frac(1, 2), b));
    CHECK(match(pat("s", "Symbol"), x, b));
    CHECK(match(pat("p", "Power"), pow(x, 2), b));
    CHECK(!match(pat("q", "Power"), x, b));
}

TEST_CASE("Pattern: repeated names must agree") {
    Bindings b;
    CHECK(match(f({pat("a"), pat("a")}), f({x, x}), b));
    Bindings c;
    CHECK(!match(f({pat("a"), pat("a")}), f({x, y}), c));
}

TEST_CASE("Pattern: sequences") {
    Bindings b;
    CHECK(match(f({pat_seq("xs")}), f({x, y, Ex(3)}), b));
    CHECK_EQ(F(b.at("xs")), std::string("Sequence[x, y, 3]"));

    Bindings c;
    CHECK(!match(f({pat_seq("xs")}), f({}), c));       // __ needs at least one
    CHECK(match(f({pat_null_seq("xs")}), f({}), c));   // ___ allows none
    CHECK_EQ(F(c.at("xs")), std::string("Sequence[]"));

    Bindings d;  // first, rest
    CHECK(match(f({pat("a"), pat_null_seq("rest")}), f({x, y, Ex(3)}), d));
    CHECK_EQ(F(d.at("a")), std::string("x"));
    CHECK_EQ(F(d.at("rest")), std::string("Sequence[y, 3]"));

    Bindings e;  // backtracking: xs__ then a literal 3 at the end
    CHECK(match(f({pat_seq("xs"), Ex(3)}), f({x, y, Ex(3)}), e));
    CHECK_EQ(F(e.at("xs")), std::string("Sequence[x, y]"));

    Bindings h;  // head-restricted sequence: all integers
    CHECK(match(f({pat_seq("ns", "Integer")}), f({Ex(1), Ex(2)}), h));
    Bindings k;
    CHECK(!match(f({pat_seq("ns", "Integer")}), f({Ex(1), x}), k));
}

TEST_CASE("Pattern: Flat and Orderless Plus matching") {
    Ex sum = plus({Ex(1).ptr(), x.ptr(), y.ptr()});

    Bindings b;
    CHECK(match(make_normal("Plus", {pat("a"), pat("b")}), sum, b));
    CHECK_EQ(F(b.at("a")), std::string("1"));
    CHECK_EQ(F(b.at("b")), std::string("Plus[x, y]"));

    Bindings c;
    CHECK(match(make_normal("Plus", {pat("rest"), Ex(1).ptr()}), sum, c));
    CHECK_EQ(F(c.at("rest")), std::string("Plus[x, y]"));

    Bindings d;
    CHECK(!match(make_normal("Plus", {pat("a"), pat("b")}),
                 make_normal("Plus", {x.ptr()}), d));
    CHECK(d.empty());

    Bindings e;
    CHECK(match(make_normal("Times", {pat("left"), pat("right")}),
                times({x.ptr(), y.ptr(), Ex(2).ptr()}), e));
    CHECK_EQ(F(e.at("left")), std::string("2"));
    CHECK_EQ(F(e.at("right")), std::string("Times[x, y]"));
}

TEST_CASE("Pattern: substitute splices sequences") {
    Bindings b;
    CHECK(match(f({pat_seq("xs")}), f({x, y}), b));
    Ex rhs = g({Ex(0), sym("xs"), Ex(9)});
    CHECK_EQ(F(substitute(rhs, b)), std::string("g[0, x, y, 9]"));
    CHECK_EQ(F(substitute(sym("unbound"), b)), std::string("unbound"));
}

TEST_CASE("Pattern: replace_all is a single top-down pass") {
    Ex rule = make_normal("Rule", {f({pat("a")}).ptr(), g({sym("a"), sym("a")}).ptr()});
    bool changed = false;
    Ex e = make_normal("List", {f({x}).ptr(), f({y}).ptr(), Ex(1).ptr()});
    CHECK_EQ(F(replace_all(e, {rule}, &changed)), std::string("List[g[x, x], g[y, y], 1]"));
    CHECK(changed);

    // Top match wins; the replacement is not rewritten again in the same pass.
    Ex nested = f({f({x})});
    CHECK_EQ(F(replace_all(nested, {rule})), std::string("g[f[x], f[x]]"));

    bool none = false;
    CHECK_EQ(F(replace_all(x, {rule}, &none)), std::string("x"));
    CHECK(!none);

    CHECK_THROWS(replace_all(x, {x}));
}

TEST_CASE("Pattern: has_pattern") {
    CHECK(has_pattern(f({pat("a")})));
    CHECK(has_pattern(blank()));
    CHECK(!has_pattern(f({x, Ex(1)})));
}
