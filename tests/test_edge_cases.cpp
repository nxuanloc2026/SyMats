// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/integer.h"
#include "symats/rational.h"
#include "symats/text.h"
#include "test.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace symats;

namespace {

// Deterministic Pseudo-Random Number Generator (LCG) for reproducible property testing.
struct SimpleRNG {
    uint64_t state = 0x123456789ABCDEF0ULL;

    uint64_t next_u64() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }

    int64_t next_i64() {
        return static_cast<int64_t>(next_u64());
    }

    // Generates an Integer with specified digit length (up to ~300 digits).
    Integer next_integer(int approx_digits, bool allow_negative = true) {
        if (approx_digits <= 0) return Integer(0);
        std::string digits;
        digits.reserve(approx_digits + 1);
        if (allow_negative && (next_u64() % 2 == 1)) {
            digits.push_back('-');
        }
        digits.push_back(static_cast<char>('1' + (next_u64() % 9)));
        for (int i = 1; i < approx_digits; ++i) {
            digits.push_back(static_cast<char>('0' + (next_u64() % 10)));
        }
        return Integer::from_string(digits);
    }

    Rational next_rational(int approx_digits) {
        Integer num = next_integer(approx_digits, true);
        Integer den = next_integer(approx_digits / 2 + 1, false);
        if (den.is_zero()) den = 1;
        return Rational(num, den);
    }
};

}  // namespace

TEST_CASE("Edge Cases: parse_text with unusual whitespace") {
    // Leading, trailing, mixed newlines, tabs, and carriage returns
    ExprPtr e1 = parse_text(" \t\n\r 2 * x \t + \n 3 * y \r\n ");
    CHECK_EQ(to_full_form(e1), std::string("Plus[Times[2, x], Times[3, y]]"));

    // Multiline expression with tabs and extra spacing inside function arguments
    ExprPtr e2 = parse_text(" \n\t Integrate[ \n\t x^2 \t , \n\t { x \t , \n\t 0 \t , \n\t 1 } \t ] \n ");
    CHECK_EQ(to_full_form(e2), std::string("Integrate[Power[x, 2], List[x, 0, 1]]"));

    // Matrix with irregular whitespace around brackets and separators
    ExprPtr e3 = parse_text(" { \n { \t 1 , \t 2 \n } \t , \n { 3 \t , 4 \n } \n } ");
    CHECK_EQ(to_full_form(e3), std::string("List[List[1, 2], List[3, 4]]"));

    // Space before parenthesis means multiplication
    ExprPtr e4 = parse_text(" x \t ( y + 1 ) ");
    CHECK_EQ(to_full_form(e4), std::string("Times[x, Plus[1, y]]"));

    // White space between relational operators and symbols
    ExprPtr e5 = parse_text(" x \t == \n y ");
    CHECK_EQ(to_full_form(e5), std::string("Equal[x, y]"));

    ExprPtr e6 = parse_text(" x \n -> \t y ");
    CHECK_EQ(to_full_form(e6), std::string("Rule[x, y]"));
}

TEST_CASE("Edge Cases: very long integers") {
    // 500-digit number parsing via parse_text and direct Integer construction
    std::string s500(500, '9'); // 999...999
    ExprPtr e_long = parse_text(s500);
    CHECK_EQ(to_text(e_long), s500);

    Integer long_int = Integer::from_string(s500);
    CHECK_EQ(long_int.to_string(), s500);

    // 1000-digit number arithmetic test
    std::string s1000_a = "1" + std::string(999, '0'); // 10^999
    std::string s1000_b = std::string(999, '9');       // 10^999 - 1
    Integer big_a = Integer::from_string(s1000_a);
    Integer big_b = Integer::from_string(s1000_b);

    CHECK_EQ(big_a - 1, big_b);
    CHECK_EQ(big_b + 1, big_a);

    // Large integer multiplication and division
    Integer big_prod = big_a * big_b;
    CHECK_EQ(big_prod / big_a, big_b);
    CHECK_EQ(big_prod / big_b, big_a);
    CHECK_EQ(big_prod % big_a, Integer(0));
}

TEST_CASE("Edge Cases: malformed input throws and does not crash") {
    // Empty and whitespace-only strings
    CHECK_THROWS(parse_text(""));
    CHECK_THROWS(parse_text("   "));
    CHECK_THROWS(parse_text("\t\n\r"));

    // Unbalanced parentheses and brackets
    CHECK_THROWS(parse_text("(x + 1"));
    CHECK_THROWS(parse_text("x + 1)"));
    CHECK_THROWS(parse_text("[1, 2"));
    CHECK_THROWS(parse_text("1, 2]"));
    CHECK_THROWS(parse_text("Sin[x,"));
    CHECK_THROWS(parse_text("f[x, y"));
    CHECK_THROWS(parse_text("{{1, 2}, {3, 4}"));

    // Incomplete or invalid binary operators
    CHECK_THROWS(parse_text("+"));
    CHECK_THROWS(parse_text("*"));
    CHECK_THROWS(parse_text("x +"));
    CHECK_THROWS(parse_text("* x"));
    CHECK_THROWS(parse_text("x / / y"));
    CHECK_THROWS(parse_text("x * * y"));
    CHECK_THROWS(parse_text("x ^^ y"));
    CHECK_THROWS(parse_text("x =="));
    CHECK_THROWS(parse_text("-> y"));
    CHECK_THROWS(parse_text("x + +"));

    // Invalid numbers and decimals
    CHECK_THROWS(parse_text("1.2.3"));
    CHECK_THROWS(parse_text("1..2"));
    CHECK_THROWS(parse_text("1."));
    CHECK_THROWS(parse_text("."));

    // Unsupported characters / mis-matched delimiters
    CHECK_THROWS(parse_text("@"));
    CHECK_THROWS(parse_text("$"));
    CHECK_THROWS(parse_text("#"));
    CHECK_THROWS(parse_text("[1, 2}"));
    CHECK_THROWS(parse_text("{1, 2]"));

    // Matrix syntax error
    CHECK_THROWS(parse_text("[1, 2; 3]"));
}

TEST_CASE("Edge Cases: Integer algebraic properties (a+b-b=a, (a*b)/b=a, gcd divides both)") {
    SimpleRNG rng;

    // Test across a variety of scales (small, 64-bit, multi-limb)
    const std::vector<int> digit_sizes = {1, 5, 18, 50, 150, 300};

    for (int digits : digit_sizes) {
        for (int i = 0; i < 20; ++i) {
            Integer a = rng.next_integer(digits, true);
            Integer b = rng.next_integer(digits, true);

            // Property 1: a + b - b = a
            CHECK_EQ(a + b - b, a);
            CHECK_EQ(a - b + b, a);

            // Property 2: (a * b) / b = a (for b != 0)
            if (!b.is_zero()) {
                Integer prod = a * b;
                CHECK_EQ(prod / b, a);
                CHECK_EQ(prod % b, Integer(0));
            }

            // Property 3: gcd divides both a and b
            Integer g = Integer::gcd(a, b);
            if (!g.is_zero()) {
                CHECK(g > Integer(0));
                CHECK_EQ(a % g, Integer(0));
                CHECK_EQ(b % g, Integer(0));
            } else {
                // gcd(0, 0) == 0
                CHECK(a.is_zero() && b.is_zero());
            }
        }
    }
}

TEST_CASE("Edge Cases: Rational algebraic properties (a+b-b=a, (a*b)/b=a)") {
    SimpleRNG rng;

    const std::vector<int> digit_sizes = {1, 5, 20, 80};

    for (int digits : digit_sizes) {
        for (int i = 0; i < 20; ++i) {
            Rational a = rng.next_rational(digits);
            Rational b = rng.next_rational(digits);

            // Property 1: a + b - b = a
            CHECK_EQ(a + b - b, a);
            CHECK_EQ(a - b + b, a);

            // Property 2: (a * b) / b = a (for b != 0)
            if (!b.is_zero()) {
                Rational prod = a * b;
                CHECK_EQ(prod / b, a);
            }
        }
    }
}
