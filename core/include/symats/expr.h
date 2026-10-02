// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// The Symats expression tree. See docs/EXPR_SPEC.md for the node vocabulary.
//
// Every value is an immutable, shared Expr of one of four kinds:
//   Integer, Rational, Symbol, Normal (head applied to arguments).
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "symats/integer.h"
#include "symats/rational.h"

namespace symats {

class Expr;
using ExprPtr = std::shared_ptr<const Expr>;
using ExprList = std::vector<ExprPtr>;

class Expr {
public:
    enum class Kind : unsigned char { Integer, Rational, Symbol, Normal };

    // Prefer the factory functions below; these constructors are public only so
    // std::make_shared can use them.
    explicit Expr(Integer v);
    explicit Expr(Rational v);          // v must not be an integer (den > 1)
    explicit Expr(std::string symbol_name);
    Expr(ExprPtr head, ExprList args);

    Kind kind() const { return kind_; }
    bool is_integer() const { return kind_ == Kind::Integer; }
    bool is_rational() const { return kind_ == Kind::Rational; }
    bool is_number() const { return kind_ == Kind::Integer || kind_ == Kind::Rational; }
    bool is_symbol() const { return kind_ == Kind::Symbol; }
    bool is_normal() const { return kind_ == Kind::Normal; }

    const Integer& integer() const;          // requires is_integer()
    const Rational& rational() const;        // requires is_rational()
    Rational number() const;                 // requires is_number(); integer -> n/1
    const std::string& name() const;         // requires is_symbol()
    const ExprPtr& head() const;             // requires is_normal()
    const ExprList& args() const;            // requires is_normal()
    std::size_t size() const { return is_normal() ? args().size() : 0; }
    const ExprPtr& arg(std::size_t i) const { return args().at(i); }

    // True if this is a Symbol with the given name.
    bool is_symbol(std::string_view n) const;
    // True if this is a Normal whose head is the Symbol `h`.
    bool has_head(std::string_view h) const;

    std::size_t hash() const { return hash_; }

private:
    struct NormalData {
        ExprPtr head;
        ExprList args;
    };
    Kind kind_;
    std::size_t hash_ = 0;
    std::variant<Integer, Rational, std::string, NormalData> data_;
};

// ---------------------------------------------------------------- construction
// Raw constructors: build exactly what you ask for (no simplification).
ExprPtr make_integer(Integer v);
ExprPtr make_number(Rational v);                       // returns an Integer when den == 1
ExprPtr make_rational(Integer num, Integer den);       // normalized; may return Integer
ExprPtr make_symbol(std::string name);
ExprPtr make_normal(ExprPtr head, ExprList args);
ExprPtr make_normal(std::string_view head, ExprList args);

// Canonical constructors: return results in canonical form (docs/EXPR_SPEC.md §2).
ExprPtr plus(ExprList terms);
ExprPtr times(ExprList factors);
ExprPtr power(const ExprPtr& base, const ExprPtr& exponent);
ExprPtr plus(const ExprPtr& a, const ExprPtr& b);
ExprPtr times(const ExprPtr& a, const ExprPtr& b);
ExprPtr negate(const ExprPtr& a);                      // -a  = Times(-1, a)
ExprPtr subtract(const ExprPtr& a, const ExprPtr& b);  // a-b = Plus(a, Times(-1, b))
ExprPtr divide(const ExprPtr& a, const ExprPtr& b);    // a/b = Times(a, Power(b, -1))

// ---------------------------------------------------------------- comparison
// Total canonical order used to sort arguments of Plus/Times (docs/EXPR_SPEC.md §2).
// Returns <0, 0, >0. Returns 0 exactly when the trees are structurally equal.
int compare(const Expr& a, const Expr& b);
inline int compare(const ExprPtr& a, const ExprPtr& b) { return compare(*a, *b); }

bool equal(const Expr& a, const Expr& b);
inline bool equal(const ExprPtr& a, const ExprPtr& b) { return a == b || equal(*a, *b); }

struct ExprLess {
    bool operator()(const ExprPtr& a, const ExprPtr& b) const { return compare(a, b) < 0; }
};

// ---------------------------------------------------------------- printing
// Internal ("full") form, e.g. Plus(1, Times(2, x)). Also valid plain-text input.
std::string to_full_form(const ExprPtr& e);

// ---------------------------------------------------------------- convenience
// Ex is a thin value wrapper with operators, for writing expressions in C++:
//   Ex x = sym("x");  Ex e = 2*x + pow(x, 2) - 1;
class Ex {
public:
    Ex(ExprPtr p) : p_(std::move(p)) {}  // NOLINT: implicit by design
    Ex(long long v) : p_(make_integer(v)) {}  // NOLINT
    Ex(int v) : p_(make_integer(v)) {}        // NOLINT

    const ExprPtr& ptr() const { return p_; }
    operator const ExprPtr&() const { return p_; }  // NOLINT
    const Expr* operator->() const { return p_.get(); }
    std::string str() const { return to_full_form(p_); }

    friend Ex operator+(const Ex& a, const Ex& b) { return plus(a.p_, b.p_); }
    friend Ex operator-(const Ex& a, const Ex& b) { return subtract(a.p_, b.p_); }
    friend Ex operator*(const Ex& a, const Ex& b) { return times(a.p_, b.p_); }
    friend Ex operator/(const Ex& a, const Ex& b) { return divide(a.p_, b.p_); }
    friend Ex operator-(const Ex& a) { return negate(a.p_); }
    friend bool operator==(const Ex& a, const Ex& b) { return equal(a.p_, b.p_); }

private:
    ExprPtr p_;
};

inline Ex sym(std::string name) { return make_symbol(std::move(name)); }
inline Ex pow(const Ex& b, const Ex& e) { return power(b.ptr(), e.ptr()); }
inline Ex frac(long long n, long long d) { return make_rational(Integer(n), Integer(d)); }

}  // namespace symats
