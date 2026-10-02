// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Exact rational number p/q, always normalized: q > 0 and gcd(p, q) = 1.
#pragma once

#include <compare>
#include <optional>
#include <string>

#include "symats/integer.h"

namespace symats {

class Rational {
public:
    Rational() : num_(0), den_(1) {}
    Rational(Integer n) : num_(std::move(n)), den_(1) {}  // NOLINT: implicit by design
    Rational(long long n) : num_(n), den_(1) {}           // NOLINT: implicit by design
    Rational(Integer n, Integer d);                       // throws std::domain_error if d == 0

    const Integer& num() const { return num_; }
    const Integer& den() const { return den_; }

    bool is_integer() const { return den_.is_one(); }
    bool is_zero() const { return num_.is_zero(); }
    bool is_one() const { return num_.is_one() && den_.is_one(); }
    int sign() const { return num_.sign(); }

    Rational operator-() const { return Rational(-num_, den_, NoNormalize{}); }
    Rational reciprocal() const;  // throws on zero

    friend Rational operator+(const Rational& a, const Rational& b);
    friend Rational operator-(const Rational& a, const Rational& b);
    friend Rational operator*(const Rational& a, const Rational& b);
    friend Rational operator/(const Rational& a, const Rational& b);

    // Exact integer power (negative exponents allowed for non-zero base).
    static Rational pow(const Rational& base, long long exp);

    // Exact rational n-th root if one exists (e.g. (8/27)^(1/3) = 2/3), else nullopt.
    std::optional<Rational> exact_root(unsigned long long n) const;

    double to_double() const { return num_.to_double() / den_.to_double(); }
    std::string to_string() const;
    std::size_t hash() const { return num_.hash() * 31u + den_.hash(); }

    friend bool operator==(const Rational& a, const Rational& b) = default;
    friend std::strong_ordering operator<=>(const Rational& a, const Rational& b);

private:
    struct NoNormalize {};
    Rational(Integer n, Integer d, NoNormalize) : num_(std::move(n)), den_(std::move(d)) {}

    Integer num_;
    Integer den_;
};

}  // namespace symats
