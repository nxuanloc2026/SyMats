// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/mathjson.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
void round_trip(std::string_view input) {
    ExprPtr first = parse_text(input);
    CHECK(equal(first, parse_mathjson(to_mathjson(first))));
}
}  // namespace

TEST_CASE("MathJSON: arithmetic and exact numbers") {
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Add",["Multiply",2,"x"],["Divide",1,3]])")),
             std::string("Plus(1/3, Times(2, x))"));
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Subtract","x","y"])")),
             to_full_form(parse_text("x-y")));
    CHECK_EQ(to_full_form(parse_mathjson(R"({"num":"1.25e2"})")), std::string("125"));
    CHECK_EQ(to_full_form(parse_mathjson(R"("0.125")")), std::string("1/8"));
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Rational",{"num":"123456789012345678901"},2])")),
             std::string("123456789012345678901/2"));
    round_trip("123456789012345678901/2 + x^2");
    round_trip("sin(x) + sqrt(2)");
    round_trip("x == 2");
    round_trip("1.25");
}

TEST_CASE("MathJSON: matrices, calculus, and object forms") {
    CHECK_EQ(to_full_form(parse_mathjson(
                 R"(["Matrix",["List",["List","a","b"],["List","c","d"]]])")),
             std::string("List(List(a, b), List(c, d))"));
    CHECK_EQ(to_full_form(parse_mathjson(
                 R"(["Integrate",["Power","x",2],["Tuple","x",0,1]])")),
             std::string("Integrate(Power(x, 2), List(x, 0, 1))"));
    CHECK_EQ(to_full_form(parse_mathjson(
                 R"(["Integrate",["Function",["Block",["Power","x",2]],"x"],["Limits","x",0,1]])")),
             std::string("Integrate(Power(x, 2), List(x, 0, 1))"));
    ExprPtr canonical_integral = parse_mathjson(
        R"(["Integrate",["Function",["Block",["Power","x",2]],"x"],["Limits","x",0,1]])");
    CHECK(equal(canonical_integral, parse_mathjson(to_mathjson(canonical_integral))));
    CHECK_EQ(to_full_form(parse_mathjson(
                 R"({"fn":["Add",{"sym":"x"},{"num":"2"}],"latex":"x+2"})")),
             std::string("Plus(2, x)"));
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Limit",["Power","x",2],"x",0])")),
             std::string("Limit(Power(x, 2), Rule(x, 0))"));
    round_trip("[[a, 1/2], [c, d]]");
    round_trip("integrate(x^2, x, 0, 1)");
    round_trip("f(x, y)");
    CHECK(to_mathjson(parse_text("integrate(x^2, x, 0, 1)")).find("Tuple") != std::string::npos);
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Ln","x"])")), std::string("Log(x)"));
    CHECK_EQ(to_full_form(parse_mathjson(R"(["Log","x",2])")), std::string("Log(2, x)"));
    round_trip("log(x)");
    round_trip("log(2, x)");
    round_trip("sum(k^2, k, 1, n)");
    round_trip("dsolve(y(x) == 0, y(x), x)");
    ExprPtr reserved = make_symbol("ExponentialE");
    CHECK(equal(reserved, parse_mathjson(to_mathjson(reserved))));
    ExprPtr unicode = make_symbol("半径");
    CHECK(equal(unicode, parse_mathjson(to_mathjson(unicode))));
    CHECK_EQ(to_full_form(parse_mathjson(R"({"num":"-Infinity"})")),
             to_full_form(negate(make_symbol("Infinity"))));
}

TEST_CASE("MathJSON: rejects invalid and unsupported input") {
    CHECK_THROWS(parse_mathjson(""));
    CHECK_THROWS(parse_mathjson(R"(["Add",1,])"));
    CHECK_THROWS(parse_mathjson(R"({"num":"NaN"})"));
    CHECK_THROWS(parse_mathjson(R"({"str":"hello"})"));
    CHECK_THROWS(parse_mathjson(R"({"fn":1})"));
    CHECK_THROWS(parse_mathjson(R"(["Rational",1,0])"));
    CHECK_THROWS(parse_mathjson(R"(["Power",1])"));
    CHECK_THROWS(parse_mathjson("true"));
    CHECK_THROWS(parse_mathjson(R"("\uD800")"));
    CHECK_THROWS(parse_mathjson(std::string(300, '[') + "0" + std::string(300, ']')));
}
