// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "linalg.h"

#include <optional>
#include <utility>
#include <vector>

#include "symats/eval.h"

namespace symats {
namespace {

using Row = std::vector<Rational>;
using Matrix = std::vector<Row>;

// Rows of a rectangular List of Lists, or nullopt.
std::optional<std::vector<ExprList>> rows(const ExprPtr& e) {
    if (!e->has_head("List") || e->size() == 0) return std::nullopt;
    std::vector<ExprList> out;
    for (const auto& r : e->args()) {
        if (!r->has_head("List") || r->size() == 0 || r->size() != e->arg(0)->size()) return std::nullopt;
        out.push_back(r->args());
    }
    return out;
}

std::optional<Matrix> exact(const ExprPtr& e) {
    auto r = rows(e);
    if (!r) return std::nullopt;
    Matrix m;
    for (const auto& row : *r) {
        Row values;
        for (const auto& x : row) {
            if (!x->is_number()) return std::nullopt;
            values.push_back(x->number());
        }
        m.push_back(std::move(values));
    }
    return m;
}

std::optional<Row> exact_vector(const ExprPtr& e) {
    if (!e->has_head("List") || e->size() == 0) return std::nullopt;
    Row v;
    for (const auto& x : e->args()) {
        if (!x->is_number()) return std::nullopt;
        v.push_back(x->number());
    }
    return v;
}

ExprPtr to_expr(const Row& v) {
    ExprList out;
    for (const auto& x : v) out.push_back(make_number(x));
    return make_normal("List", std::move(out));
}

ExprPtr to_expr(const Matrix& m) {
    ExprList out;
    for (const auto& r : m) out.push_back(to_expr(r));
    return make_normal("List", std::move(out));
}

// Gauss-Jordan elimination to reduced row echelon form. Returns the pivot columns;
// `det` (if given) receives the determinant for square input.
std::vector<std::size_t> reduce(Matrix& m, Rational* det = nullptr) {
    const std::size_t nr = m.size(), nc = m[0].size();
    Rational d = 1;
    std::vector<std::size_t> pivots;
    std::size_t r = 0;
    for (std::size_t c = 0; c < nc && r < nr; ++c) {
        std::size_t p = r;
        while (p < nr && m[p][c].is_zero()) ++p;
        if (p == nr) continue;
        if (p != r) {
            std::swap(m[p], m[r]);
            d = -d;
        }
        const Rational pivot = m[r][c];
        d = d * pivot;
        for (auto& x : m[r]) x = x / pivot;
        for (std::size_t i = 0; i < nr; ++i) {
            if (i == r || m[i][c].is_zero()) continue;
            const Rational f = m[i][c];
            for (std::size_t j = c; j < nc; ++j) m[i][j] = m[i][j] - f * m[r][j];
        }
        pivots.push_back(c);
        ++r;
    }
    if (det) *det = pivots.size() == nr && nr == nc ? d : Rational(0);
    return pivots;
}

using Builtin = Context::Builtin;

ExprPtr transpose(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto r = rows(e->arg(0));
    if (!r) return nullptr;
    ExprList out;
    for (std::size_t j = 0; j < (*r)[0].size(); ++j) {
        ExprList col;
        for (const auto& row : *r) col.push_back(row[j]);
        out.push_back(make_normal("List", std::move(col)));
    }
    return make_normal("List", std::move(out));
}

ExprPtr trace(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto r = rows(e->arg(0));
    if (!r || r->size() != (*r)[0].size()) return nullptr;
    ExprList diagonal;
    for (std::size_t i = 0; i < r->size(); ++i) diagonal.push_back((*r)[i][i]);
    return plus(std::move(diagonal));
}

ExprPtr identity_matrix(const ExprPtr& e, Context&) {
    if (e->size() != 1 || !e->arg(0)->is_integer()) return nullptr;
    const auto n = e->arg(0)->integer().to_int64();
    if (!n || *n < 1 || *n > 10000) return nullptr;
    Matrix m(static_cast<std::size_t>(*n), Row(static_cast<std::size_t>(*n), Rational(0)));
    for (std::size_t i = 0; i < m.size(); ++i) m[i][i] = 1;
    return to_expr(m);
}

ExprPtr det(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto m = exact(e->arg(0));
    if (!m || m->size() != (*m)[0].size()) return nullptr;
    Rational d;
    reduce(*m, &d);
    return make_number(d);
}

ExprPtr inverse(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto m = exact(e->arg(0));
    if (!m || m->size() != (*m)[0].size()) return nullptr;
    const std::size_t n = m->size();
    for (std::size_t i = 0; i < n; ++i) {  // augment with the identity
        (*m)[i].resize(2 * n, Rational(0));
        (*m)[i][n + i] = 1;
    }
    const auto pivots = reduce(*m);
    if (pivots.size() < n || pivots[n - 1] != n - 1) return nullptr;  // singular: stays unevaluated
    Matrix inv(n);
    for (std::size_t i = 0; i < n; ++i) inv[i].assign((*m)[i].begin() + static_cast<std::ptrdiff_t>(n), (*m)[i].end());
    return to_expr(inv);
}

ExprPtr rank(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto m = exact(e->arg(0));
    if (!m) return nullptr;
    return make_integer(static_cast<long long>(reduce(*m).size()));
}

ExprPtr row_reduce(const ExprPtr& e, Context&) {
    if (e->size() != 1) return nullptr;
    auto m = exact(e->arg(0));
    if (!m) return nullptr;
    reduce(*m);
    return to_expr(*m);
}

// The unique solution of A . x == b; underdetermined or inconsistent systems stay unevaluated.
ExprPtr linear_solve(const ExprPtr& e, Context&) {
    if (e->size() != 2) return nullptr;
    auto m = exact(e->arg(0));
    auto b = exact_vector(e->arg(1));
    if (!m || !b || b->size() != m->size()) return nullptr;
    const std::size_t nc = (*m)[0].size();
    for (std::size_t i = 0; i < m->size(); ++i) (*m)[i].push_back((*b)[i]);
    const auto pivots = reduce(*m);
    if (pivots.size() != nc || (!pivots.empty() && pivots.back() == nc)) return nullptr;
    Row x(nc);
    for (std::size_t i = 0; i < nc; ++i) x[pivots[i]] = (*m)[i][nc];
    return to_expr(x);
}

// Characteristic polynomial Det[x I - A] by Faddeev-LeVerrier (exact for rationals).
ExprPtr char_poly(const ExprPtr& e, Context&) {
    if (e->size() != 2) return nullptr;
    auto a = exact(e->arg(0));
    if (!a || a->size() != (*a)[0].size()) return nullptr;
    const std::size_t n = a->size();
    // M_0 = 0, c_n = 1; M_k = A M_{k-1} + c_{n-k+1} I, c_{n-k} = -Tr(A M_k) / k.
    std::vector<Rational> c(n + 1, Rational(0));
    c[n] = 1;
    Matrix m(n, Row(n, Rational(0)));
    for (std::size_t k = 1; k <= n; ++k) {
        Matrix am(n, Row(n, Rational(0)));
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) {
                Rational s = 0;
                for (std::size_t l = 0; l < n; ++l) s = s + (*a)[i][l] * m[l][j];
                am[i][j] = s;
            }
        for (std::size_t i = 0; i < n; ++i) am[i][i] = am[i][i] + c[n - k + 1];
        m = std::move(am);
        Rational tr = 0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t l = 0; l < n; ++l) tr = tr + (*a)[i][l] * m[l][i];
        c[n - k] = -tr / Rational(static_cast<long long>(k));
    }
    ExprList terms;
    for (std::size_t k = 0; k <= n; ++k)
        terms.push_back(times(make_number(c[k]), power(e->arg(1), make_integer(static_cast<long long>(k)))));
    return plus(std::move(terms));
}

}  // namespace

void install_linear_algebra(Context& ctx) {
    const std::pair<const char*, ExprPtr (*)(const ExprPtr&, Context&)> table[] = {
        {"Transpose", transpose}, {"Trace", trace},          {"IdentityMatrix", identity_matrix},
        {"Det", det},             {"Inverse", inverse},      {"Rank", rank},
        {"RowReduce", row_reduce}, {"LinearSolve", linear_solve}, {"CharPoly", char_poly},
    };
    for (const auto& [name, fn] : table) {
        ctx.set_builtin(name, fn);
        ctx.set_attributes(name, attr::Protected);
    }
}

}  // namespace symats
