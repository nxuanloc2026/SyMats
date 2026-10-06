// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Built-in arbitrary-precision integer (schoolbook algorithms).
// Reference: D. E. Knuth, The Art of Computer Programming, Vol. 2, Section 4.3.1.
#include "symats/integer.h"

#include <algorithm>
#include <climits>
#include <stdexcept>

namespace symats {

namespace {
template <class V>
void trim_vec(V& v) {
    while (!v.empty() && v.back() == 0) v.pop_back();
}
}  // namespace

Integer::Integer(long long v) {
    unsigned long long mag = v < 0 ? 0ULL - static_cast<unsigned long long>(v)
                                   : static_cast<unsigned long long>(v);
    neg_ = v < 0;
    while (mag != 0) {
        limbs_.push_back(static_cast<Limb>(mag % kBase));
        mag /= kBase;
    }
}

Integer Integer::from_string(std::string_view s) {
    Integer r;
    std::size_t pos = 0;
    bool neg = false;
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
        neg = s[pos] == '-';
        ++pos;
    }
    if (pos == s.size()) throw std::invalid_argument("Integer::from_string: no digits");
    for (std::size_t i = pos; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9')
            throw std::invalid_argument("Integer::from_string: invalid character");
    }
    // Read 9-digit chunks from the right.
    std::size_t end = s.size();
    while (end > pos) {
        std::size_t start = end >= pos + kBaseDigits ? end - kBaseDigits : pos;
        Limb chunk = 0;
        for (std::size_t i = start; i < end; ++i) chunk = chunk * 10 + static_cast<Limb>(s[i] - '0');
        r.limbs_.push_back(chunk);
        end = start;
    }
    r.trim();
    r.neg_ = neg && !r.is_zero();
    return r;
}

void Integer::trim() {
    trim_vec(limbs_);
    if (limbs_.empty()) neg_ = false;
}

int Integer::cmp_abs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (std::size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::vector<Integer::Limb> Integer::add_abs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    std::vector<Limb> r;
    r.reserve(std::max(a.size(), b.size()) + 1);
    Limb carry = 0;
    for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        std::uint64_t s = static_cast<std::uint64_t>(carry) + (i < a.size() ? a[i] : 0) +
                          (i < b.size() ? b[i] : 0);
        r.push_back(static_cast<Limb>(s % kBase));
        carry = static_cast<Limb>(s / kBase);
    }
    if (carry) r.push_back(carry);
    return r;
}

// Requires |a| >= |b|.
std::vector<Integer::Limb> Integer::sub_abs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    std::vector<Limb> r(a.size());
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::int64_t d = static_cast<std::int64_t>(a[i]) - borrow - (i < b.size() ? b[i] : 0);
        borrow = d < 0 ? 1 : 0;
        if (d < 0) d += kBase;
        r[i] = static_cast<Limb>(d);
    }
    trim_vec(r);
    return r;
}

std::vector<Integer::Limb> Integer::mul_abs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<std::uint64_t> acc(a.size() + b.size(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
            std::uint64_t cur = acc[i + j] + static_cast<std::uint64_t>(a[i]) * b[j] + carry;
            acc[i + j] = cur % kBase;
            carry = cur / kBase;
        }
        std::size_t k = i + b.size();
        while (carry) {
            std::uint64_t cur = acc[k] + carry;
            acc[k] = cur % kBase;
            carry = cur / kBase;
            ++k;
        }
    }
    std::vector<Limb> r(acc.begin(), acc.end());
    trim_vec(r);
    return r;
}

std::vector<Integer::Limb> Integer::mul_small(const std::vector<Limb>& a, Limb m) {
    if (a.empty() || m == 0) return {};
    std::vector<Limb> r;
    r.reserve(a.size() + 1);
    std::uint64_t carry = 0;
    for (Limb x : a) {
        std::uint64_t cur = static_cast<std::uint64_t>(x) * m + carry;
        r.push_back(static_cast<Limb>(cur % kBase));
        carry = cur / kBase;
    }
    if (carry) r.push_back(static_cast<Limb>(carry));
    return r;
}

std::pair<std::vector<Integer::Limb>, std::vector<Integer::Limb>>
Integer::divmod_abs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    if (b.empty()) throw std::domain_error("Integer: division by zero");
    if (cmp_abs(a, b) < 0) return {{}, a};

    if (b.size() == 1) {  // fast path: single-limb divisor
        std::vector<Limb> q(a.size());
        std::uint64_t rem = 0;
        for (std::size_t i = a.size(); i-- > 0;) {
            std::uint64_t cur = rem * kBase + a[i];
            q[i] = static_cast<Limb>(cur / b[0]);
            rem = cur % b[0];
        }
        trim_vec(q);
        std::vector<Limb> r;
        if (rem) r.push_back(static_cast<Limb>(rem));
        return {q, r};
    }

    // Long division, one base-10^9 digit at a time; each digit found by binary search.
    std::vector<Limb> q(a.size(), 0);
    std::vector<Limb> rem;
    for (std::size_t i = a.size(); i-- > 0;) {
        rem.insert(rem.begin(), a[i]);
        trim_vec(rem);
        Limb lo = 0, hi = kBase - 1;
        while (lo < hi) {
            Limb mid = lo + (hi - lo + 1) / 2;
            if (cmp_abs(mul_small(b, mid), rem) <= 0) lo = mid;
            else hi = mid - 1;
        }
        q[i] = lo;
        if (lo) rem = sub_abs(rem, mul_small(b, lo));
    }
    trim_vec(q);
    return {q, rem};
}

Integer Integer::abs() const {
    Integer r = *this;
    r.neg_ = false;
    return r;
}

Integer Integer::operator-() const {
    Integer r = *this;
    if (!r.is_zero()) r.neg_ = !r.neg_;
    return r;
}

Integer& Integer::operator+=(const Integer& o) {
    if (neg_ == o.neg_) {
        limbs_ = add_abs(limbs_, o.limbs_);
    } else if (cmp_abs(limbs_, o.limbs_) >= 0) {
        limbs_ = sub_abs(limbs_, o.limbs_);
    } else {
        limbs_ = sub_abs(o.limbs_, limbs_);
        neg_ = o.neg_;
    }
    trim();
    return *this;
}

Integer& Integer::operator-=(const Integer& o) { return *this += -o; }

Integer& Integer::operator*=(const Integer& o) {
    neg_ = neg_ != o.neg_;
    limbs_ = mul_abs(limbs_, o.limbs_);
    trim();
    return *this;
}

std::pair<Integer, Integer> Integer::divmod(const Integer& a, const Integer& b) {
    auto [qa, ra] = divmod_abs(a.limbs_, b.limbs_);
    Integer q, r;
    q.limbs_ = std::move(qa);
    q.neg_ = a.neg_ != b.neg_;
    q.trim();
    r.limbs_ = std::move(ra);
    r.neg_ = a.neg_;
    r.trim();
    return {q, r};
}

Integer& Integer::operator/=(const Integer& o) { return *this = divmod(*this, o).first; }
Integer& Integer::operator%=(const Integer& o) { return *this = divmod(*this, o).second; }

Integer Integer::gcd(const Integer& a, const Integer& b) {
    Integer x = a.abs(), y = b.abs();
    while (!y.is_zero()) {
        Integer r = divmod(x, y).second;
        x = std::move(y);
        y = std::move(r);
    }
    return x;
}

Integer Integer::pow(const Integer& base, unsigned long long exp) {
    Integer result = 1, b = base;
    while (exp) {
        if (exp & 1ULL) result *= b;
        exp >>= 1;
        if (exp) b *= b;
    }
    return result;
}

std::pair<Integer, bool> Integer::iroot(unsigned long long n) const {
    if (n == 0) throw std::domain_error("Integer::iroot: n must be positive");
    if (neg_) throw std::domain_error("Integer::iroot: negative argument");
    if (n == 1 || is_zero() || is_one()) return {*this, true};

    const std::size_t digits = to_string().size();
    // 2^n > *this whenever n > log2(*this); log2(*this) < 3.33 * digits.
    if (n > 4ULL * digits) return {Integer(1), false};

    Integer lo = 0;
    Integer hi = pow(Integer(10), digits / n + 1);
    const Integer two = 2;
    while (lo < hi) {
        Integer mid = (lo + hi + 1) / two;
        if (pow(mid, n) <= *this) lo = mid;
        else hi = mid - 1;
    }
    bool exact = pow(lo, n) == *this;
    return {lo, exact};
}

std::optional<long long> Integer::to_int64() const {
    if (limbs_.size() > 3) return std::nullopt;
    unsigned long long mag = 0;
    for (std::size_t i = limbs_.size(); i-- > 0;) {
        if (mag > (ULLONG_MAX - limbs_[i]) / kBase) return std::nullopt;
        mag = mag * kBase + limbs_[i];
    }
    const unsigned long long lim = static_cast<unsigned long long>(LLONG_MAX);
    if (!neg_) {
        if (mag > lim) return std::nullopt;
        return static_cast<long long>(mag);
    }
    if (mag > lim + 1) return std::nullopt;
    if (mag == lim + 1) return LLONG_MIN;
    return -static_cast<long long>(mag);
}

double Integer::to_double() const {
    double r = 0;
    for (std::size_t i = limbs_.size(); i-- > 0;) r = r * kBase + limbs_[i];
    return neg_ ? -r : r;
}

std::string Integer::to_string() const {
    if (is_zero()) return "0";
    std::string s = neg_ ? "-" : "";
    s += std::to_string(limbs_.back());
    for (std::size_t i = limbs_.size() - 1; i-- > 0;) {
        std::string part = std::to_string(limbs_[i]);
        s.append(kBaseDigits - part.size(), '0');
        s += part;
    }
    return s;
}

std::size_t Integer::hash() const {
    std::size_t h = neg_ ? 0x9e3779b97f4a7c15ULL : 0x84222325cbf29ce4ULL;
    for (Limb l : limbs_) h = (h ^ l) * 0x100000001b3ULL;
    return h;
}

std::strong_ordering operator<=>(const Integer& a, const Integer& b) {
    if (a.neg_ != b.neg_) return a.neg_ ? std::strong_ordering::less : std::strong_ordering::greater;
    int c = Integer::cmp_abs(a.limbs_, b.limbs_);
    if (a.neg_) c = -c;
    return c < 0 ? std::strong_ordering::less
                 : (c > 0 ? std::strong_ordering::greater : std::strong_ordering::equal);
}

}  // namespace symats
