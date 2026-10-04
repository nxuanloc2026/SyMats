// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Arbitrary-precision signed integer.
//
// v0.1 uses a small built-in implementation (base 10^9 limbs, schoolbook algorithms)
// so the project builds with zero external dependencies. The public interface is
// designed so a GMP (mpz) backend can replace the internals later without changing
// any calling code.
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef SYMATS_USE_GMP
#include <gmpxx.h>
#endif

namespace symats {

class Integer {
public:
    Integer() = default;
    Integer(long long v);  // NOLINT(google-explicit-constructor): implicit by design

    // Parses an optional sign followed by decimal digits. Throws std::invalid_argument.
    static Integer from_string(std::string_view s);

    bool is_zero() const;
    bool is_one() const;
    bool is_negative() const;
    int sign() const { return is_zero() ? 0 : (is_negative() ? -1 : 1); }
    bool is_even() const;

    Integer abs() const;
    Integer operator-() const;

    Integer& operator+=(const Integer& o);
    Integer& operator-=(const Integer& o);
    Integer& operator*=(const Integer& o);
    Integer& operator/=(const Integer& o);  // truncates toward zero
    Integer& operator%=(const Integer& o);  // remainder has the sign of the dividend

    friend Integer operator+(Integer a, const Integer& b) { return a += b; }
    friend Integer operator-(Integer a, const Integer& b) { return a -= b; }
    friend Integer operator*(Integer a, const Integer& b) { return a *= b; }
    friend Integer operator/(Integer a, const Integer& b) { return a /= b; }
    friend Integer operator%(Integer a, const Integer& b) { return a %= b; }

    // Truncated division: a = q*b + r, |r| < |b|, sign(r) = sign(a). Throws on b == 0.
    static std::pair<Integer, Integer> divmod(const Integer& a, const Integer& b);

    static Integer gcd(const Integer& a, const Integer& b);  // always >= 0
    static Integer pow(const Integer& base, unsigned long long exp);

    // Floor of the n-th root of a non-negative integer, and whether it is exact.
    // Throws std::domain_error if *this < 0 or n == 0.
    std::pair<Integer, bool> iroot(unsigned long long n) const;

    std::optional<long long> to_int64() const;
    double to_double() const;
    std::string to_string() const;
    std::size_t hash() const;

#ifdef SYMATS_USE_GMP
    friend bool operator==(const Integer& a, const Integer& b) { return a.value_ == b.value_; }
    friend std::strong_ordering operator<=>(const Integer& a, const Integer& b) {
        int cmp = mpz_cmp(a.value_.get_mpz_t(), b.value_.get_mpz_t());
        return cmp < 0 ? std::strong_ordering::less
                       : (cmp > 0 ? std::strong_ordering::greater : std::strong_ordering::equal);
    }
#else
    friend bool operator==(const Integer& a, const Integer& b) = default;
    friend std::strong_ordering operator<=>(const Integer& a, const Integer& b);
#endif

private:
#ifdef SYMATS_USE_GMP
    mpz_class value_{0};
#else
    using Limb = std::uint32_t;
    static constexpr Limb kBase = 1000000000u;  // 10^9
    static constexpr int kBaseDigits = 9;

    bool neg_ = false;          // never true for zero
    std::vector<Limb> limbs_;   // little-endian, no leading zero limbs

    void trim();
    static int cmp_abs(const std::vector<Limb>& a, const std::vector<Limb>& b);
    static std::vector<Limb> add_abs(const std::vector<Limb>& a, const std::vector<Limb>& b);
    static std::vector<Limb> sub_abs(const std::vector<Limb>& a, const std::vector<Limb>& b);
    static std::vector<Limb> mul_abs(const std::vector<Limb>& a, const std::vector<Limb>& b);
    static std::vector<Limb> mul_small(const std::vector<Limb>& a, Limb m);
    static std::pair<std::vector<Limb>, std::vector<Limb>>
    divmod_abs(const std::vector<Limb>& a, const std::vector<Limb>& b);
#endif
};

}  // namespace symats
