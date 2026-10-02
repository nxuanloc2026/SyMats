// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/rational.h"

#include <stdexcept>

namespace symats {

Rational::Rational(Integer n, Integer d) {
    if (d.is_zero()) throw std::domain_error("Rational: zero denominator");
    if (d.is_negative()) {
        n = -n;
        d = -d;
    }
    Integer g = Integer::gcd(n, d);
    if (!g.is_one() && !g.is_zero()) {
        n /= g;
        d /= g;
    }
    num_ = std::move(n);
    den_ = std::move(d);
}

Rational Rational::reciprocal() const {
    if (num_.is_zero()) throw std::domain_error("Rational: reciprocal of zero");
    return Rational(den_, num_);
}

Rational operator+(const Rational& a, const Rational& b) {
    if (a.den_.is_one() && b.den_.is_one()) return Rational(a.num_ + b.num_);
    return Rational(a.num_ * b.den_ + b.num_ * a.den_, a.den_ * b.den_);
}

Rational operator-(const Rational& a, const Rational& b) { return a + (-b); }

Rational operator*(const Rational& a, const Rational& b) {
    if (a.den_.is_one() && b.den_.is_one()) return Rational(a.num_ * b.num_);
    return Rational(a.num_ * b.num_, a.den_ * b.den_);
}

Rational operator/(const Rational& a, const Rational& b) { return a * b.reciprocal(); }

Rational Rational::pow(const Rational& base, long long exp) {
    if (exp == 0) return Rational(1);
    bool invert = exp < 0;
    unsigned long long e = invert ? 0ULL - static_cast<unsigned long long>(exp)
                                  : static_cast<unsigned long long>(exp);
    Rational r(Integer::pow(base.num_, e), Integer::pow(base.den_, e), NoNormalize{});
    return invert ? r.reciprocal() : r;
}

std::optional<Rational> Rational::exact_root(unsigned long long n) const {
    if (n == 0) return std::nullopt;
    if (n == 1) return *this;
    bool neg = num_.is_negative();
    if (neg && n % 2 == 0) return std::nullopt;  // no real even root of a negative number
    auto [rn, okn] = num_.abs().iroot(n);
    if (!okn) return std::nullopt;
    auto [rd, okd] = den_.iroot(n);
    if (!okd) return std::nullopt;
    return Rational(neg ? -rn : rn, rd, NoNormalize{});
}

std::string Rational::to_string() const {
    if (den_.is_one()) return num_.to_string();
    return num_.to_string() + "/" + den_.to_string();
}

std::strong_ordering operator<=>(const Rational& a, const Rational& b) {
    // Denominators are positive, so cross-multiplication preserves order.
    return a.num_ * b.den_ <=> b.num_ * a.den_;
}

}  // namespace symats
