// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/expr.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace symats {

namespace {
std::size_t mix(std::size_t h, std::size_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}
}  // namespace

// ---------------------------------------------------------------- Expr

Expr::Expr(Integer v) : kind_(Kind::Integer), data_(std::move(v)) {
    hash_ = mix(1, std::get<Integer>(data_).hash());
}

Expr::Expr(Rational v) : kind_(Kind::Rational), data_(std::move(v)) {
    if (std::get<Rational>(data_).is_integer())
        throw std::invalid_argument("Expr: integer-valued Rational; use make_number");
    hash_ = mix(2, std::get<Rational>(data_).hash());
}

Expr::Expr(std::string symbol_name) : kind_(Kind::Symbol), data_(std::move(symbol_name)) {
    if (std::get<std::string>(data_).empty()) throw std::invalid_argument("Expr: empty symbol name");
    hash_ = mix(3, std::hash<std::string>{}(std::get<std::string>(data_)));
}

Expr::Expr(ExprPtr head, ExprList args) : kind_(Kind::Normal) {
    if (!head) throw std::invalid_argument("Expr: null head");
    std::size_t h = mix(4, head->hash());
    for (const auto& a : args) {
        if (!a) throw std::invalid_argument("Expr: null argument");
        h = mix(h, a->hash());
    }
    hash_ = h;
    data_ = NormalData{std::move(head), std::move(args)};
}

const Integer& Expr::integer() const { return std::get<Integer>(data_); }
const Rational& Expr::rational() const { return std::get<Rational>(data_); }
const std::string& Expr::name() const { return std::get<std::string>(data_); }
const ExprPtr& Expr::head() const { return std::get<NormalData>(data_).head; }
const ExprList& Expr::args() const { return std::get<NormalData>(data_).args; }

Rational Expr::number() const {
    if (kind_ == Kind::Integer) return Rational(integer());
    return rational();
}

bool Expr::is_symbol(std::string_view n) const { return kind_ == Kind::Symbol && name() == n; }

bool Expr::has_head(std::string_view h) const {
    return kind_ == Kind::Normal && head()->is_symbol(h);
}

// ---------------------------------------------------------------- factories

ExprPtr make_integer(Integer v) { return std::make_shared<const Expr>(std::move(v)); }

ExprPtr make_number(Rational v) {
    if (v.is_integer()) return make_integer(v.num());
    return std::make_shared<const Expr>(std::move(v));
}

ExprPtr make_rational(Integer num, Integer den) {
    return make_number(Rational(std::move(num), std::move(den)));
}

ExprPtr make_symbol(std::string name) { return std::make_shared<const Expr>(std::move(name)); }

ExprPtr make_normal(ExprPtr head, ExprList args) {
    return std::make_shared<const Expr>(std::move(head), std::move(args));
}

ExprPtr make_normal(std::string_view head, ExprList args) {
    return make_normal(make_symbol(std::string(head)), std::move(args));
}

// ---------------------------------------------------------------- ordering
//
// Canonical order (docs/EXPR_SPEC.md §2):
//   numbers (by value) < symbols (by name) < normal expressions (by head, then args).
// Powers sort by their base first, so x < x^2 < x^3 < y. A power sorts right after
// its own base.

namespace {

int sign_of(std::strong_ordering o) {
    return o < 0 ? -1 : (o > 0 ? 1 : 0);
}

int rank(const Expr& e) {
    if (e.is_number()) return 0;
    if (e.is_symbol()) return 1;
    return 2;
}

int compare_plain(const Expr& a, const Expr& b);

int compare_args(const ExprList& x, const ExprList& y) {
    std::size_t n = std::min(x.size(), y.size());
    for (std::size_t i = 0; i < n; ++i) {
        int c = compare(*x[i], *y[i]);
        if (c) return c;
    }
    if (x.size() != y.size()) return x.size() < y.size() ? -1 : 1;
    return 0;
}

// Order ignoring the special Power rule.
int compare_plain(const Expr& a, const Expr& b) {
    int ra = rank(a), rb = rank(b);
    if (ra != rb) return ra < rb ? -1 : 1;
    switch (ra) {
        case 0: {
            int c = sign_of(a.number() <=> b.number());
            if (c) return c;
            // Same value: canonical numbers never collide, but keep the order total.
            if (a.kind() != b.kind()) return a.is_integer() ? -1 : 1;
            return 0;
        }
        case 1:
            return a.name() < b.name() ? -1 : (a.name() > b.name() ? 1 : 0);
        default: {
            int c = compare(*a.head(), *b.head());
            if (c) return c;
            return compare_args(a.args(), b.args());
        }
    }
}

bool is_power(const Expr& e) { return e.has_head("Power") && e.size() == 2; }

}  // namespace

int compare(const Expr& a, const Expr& b) {
    if (&a == &b) return 0;
    const bool pa = is_power(a), pb = is_power(b);
    if (pa && pb) {
        int c = compare(*a.arg(0), *b.arg(0));
        if (c) return c;
        return compare(*a.arg(1), *b.arg(1));
    }
    if (pa && !b.is_number()) {
        int c = compare(*a.arg(0), b);
        return c ? c : 1;  // x^n after x
    }
    if (pb && !a.is_number()) {
        int c = compare(a, *b.arg(0));
        return c ? c : -1;
    }
    return compare_plain(a, b);
}

bool equal(const Expr& a, const Expr& b) {
    if (&a == &b) return true;
    if (a.hash() != b.hash() || a.kind() != b.kind()) return false;
    switch (a.kind()) {
        case Expr::Kind::Integer: return a.integer() == b.integer();
        case Expr::Kind::Rational: return a.rational() == b.rational();
        case Expr::Kind::Symbol: return a.name() == b.name();
        case Expr::Kind::Normal: {
            if (!equal(a.head(), b.head()) || a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i)
                if (!equal(a.arg(i), b.arg(i))) return false;
            return true;
        }
    }
    return false;
}

}  // namespace symats
