// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/verification.h"

#include <cmath>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/numeric.h"
#include "symats/pattern.h"

namespace symats {
namespace {

// When set, identities that the exact check cannot prove are accepted if they hold
// numerically at several sample points (the Numeric verification level).
thread_local bool numeric_mode = false;

void free_symbols(const ExprPtr& e, std::set<std::string>& out) {
    if (e->is_symbol()) {
        if (!e->is_symbol("Pi") && !e->is_symbol("E")) out.insert(e->name());
    } else if (e->is_normal()) {
        free_symbols(e->head(), out);
        for (const auto& a : e->args()) free_symbols(a, out);
    }
}

// Sample assignments for the free symbols: fixed seed, values in [0.2, 2.2] so that
// Log and Sqrt stay real. Returns the slot map; each sample is one vector.
numeric::Slots sample_points(const std::set<std::string>& symbols,
                             std::vector<std::vector<double>>& samples) {
    numeric::Slots slots;
    for (const auto& s : symbols) slots.emplace(s, slots.size());
    std::mt19937_64 rng(20261009);
    std::uniform_real_distribution<double> uniform(0.2, 2.2);
    samples.assign(6, std::vector<double>(slots.size()));
    for (auto& sample : samples)
        for (auto& v : sample) v = uniform(rng);
    return slots;
}

bool close(double a, double b, double tol) {
    return std::abs(a - b) <= tol * (1.0 + std::abs(a) + std::abs(b));
}

// lhs == rhs at every sample point where both are finite (at least 4 of 6).
bool numeric_identity(const ExprPtr& lhs, const ExprPtr& rhs) {
    std::set<std::string> symbols;
    free_symbols(lhs, symbols);
    free_symbols(rhs, symbols);
    std::vector<std::vector<double>> samples;
    const numeric::Slots slots = sample_points(symbols, samples);
    try {
        const auto l = numeric::compile(lhs, slots), r = numeric::compile(rhs, slots);
        int valid = 0;
        for (const auto& sample : samples) {
            const double a = l.eval(sample.data()), b = r.eval(sample.data());
            if (!std::isfinite(a) || !std::isfinite(b)) continue;
            if (!close(a, b, 1e-9)) return false;
            ++valid;
        }
        return valid >= 4;
    } catch (const numeric::Unsupported&) {
        return false;
    }
}

// Adaptive Simpson quadrature for checking definite integrals.
double simpson(const std::function<double(double)>& f, double a, double b, double fa, double fm,
               double fb, double whole, double tol, int depth) {
    const double m = (a + b) / 2, lm = (a + m) / 2, rm = (m + b) / 2;
    const double flm = f(lm), frm = f(rm);
    const double left = (m - a) / 6 * (fa + 4 * flm + fm), right = (b - m) / 6 * (fm + 4 * frm + fb);
    if (depth <= 0 || std::abs(left + right - whole) <= 15 * tol)
        return left + right + (left + right - whole) / 15;
    return simpson(f, a, m, fa, flm, fm, left, tol / 2, depth - 1) +
           simpson(f, m, b, fm, frm, fb, right, tol / 2, depth - 1);
}

// Integrate[f, {x, a, b}] == result, by quadrature at sampled values of any parameters.
bool numeric_definite_integral(const ExprPtr& f, const ExprPtr& range, const ExprPtr& result) {
    if (!range->has_head("List") || range->size() != 3 || !range->arg(0)->is_symbol()) return false;
    std::set<std::string> symbols;
    for (const auto& e : {f, range->arg(1), range->arg(2), result}) free_symbols(e, symbols);
    const std::string x = range->arg(0)->name();
    symbols.erase(x);
    std::vector<std::vector<double>> samples;
    numeric::Slots slots = sample_points(symbols, samples);
    try {
        const auto lower = numeric::compile(range->arg(1), slots);
        const auto upper = numeric::compile(range->arg(2), slots);
        const auto value = numeric::compile(result, slots);
        const std::size_t xi = slots.size();
        slots.emplace(x, xi);
        const auto integrand = numeric::compile(f, slots);
        int valid = 0;
        for (auto sample : samples) {
            sample.push_back(0.0);
            const double a = lower.eval(sample.data()), b = upper.eval(sample.data());
            const double expected = value.eval(sample.data());
            const auto g = [&](double t) {
                sample[xi] = t;
                return integrand.eval(sample.data());
            };
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(expected)) continue;
            const double fa = g(a), fm = g((a + b) / 2), fb = g(b);
            if (!std::isfinite(fa) || !std::isfinite(fm) || !std::isfinite(fb)) continue;
            const double whole = (b - a) / 6 * (fa + 4 * fm + fb);
            const double q = simpson(g, a, b, fa, fm, fb, whole, 1e-11 * (1 + std::abs(whole)), 30);
            if (!std::isfinite(q)) continue;
            if (!close(q, expected, 1e-7)) return false;
            ++valid;
        }
        return valid >= 4;
    } catch (const numeric::Unsupported&) {
        return false;
    }
}

ExprPtr normalize(const ExprPtr& expression) {
    try {
        Context context;
        return evaluate(expression, context);
    } catch (...) {
        return expression;
    }
}

bool zero(const ExprPtr& expression) {
    const ExprPtr normalized = normalize(expression);
    return normalized->is_integer() && normalized->integer().is_zero();
}

bool identity(const ExprPtr& lhs, const ExprPtr& rhs) {
    if (zero(subtract(lhs, rhs))) return true;
    return numeric_mode && numeric_identity(normalize(lhs), normalize(rhs));
}

// Structural equality, or entrywise identity for lists (numerically in numeric_mode).
bool same(const ExprPtr& lhs, const ExprPtr& rhs) {
    if (equal(lhs, rhs)) return true;
    if (lhs->has_head("List") && rhs->has_head("List")) {
        if (lhs->size() != rhs->size()) return false;
        for (std::size_t i = 0; i < lhs->size(); ++i)
            if (!same(lhs->arg(i), rhs->arg(i))) return false;
        return true;
    }
    return !lhs->has_head("List") && !rhs->has_head("List") && identity(lhs, rhs);
}

bool matrix(const ExprPtr& expression, std::size_t* rows = nullptr,
            std::size_t* columns = nullptr) {
    if (!expression->has_head("List") || expression->size() == 0) return false;
    const std::size_t cols = expression->arg(0)->size();
    if (cols == 0) return false;
    for (const auto& row : expression->args())
        if (!row->has_head("List") || row->size() != cols) return false;
    if (rows) *rows = expression->size();
    if (columns) *columns = cols;
    return true;
}

ExprPtr matrix_product(const ExprPtr& lhs, const ExprPtr& rhs) {
    std::size_t rows = 0, inner = 0, columns = 0, rhs_rows = 0;
    if (!matrix(lhs, &rows, &inner) || !matrix(rhs, &rhs_rows, &columns) ||
        inner != rhs_rows)
        return nullptr;
    ExprList values;
    values.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i) {
        ExprList row;
        row.reserve(columns);
        for (std::size_t j = 0; j < columns; ++j) {
            ExprList terms;
            for (std::size_t k = 0; k < inner; ++k)
                terms.push_back(times(lhs->arg(i)->arg(k), rhs->arg(k)->arg(j)));
            row.push_back(plus(std::move(terms)));
        }
        values.push_back(make_normal("List", std::move(row)));
    }
    return make_normal("List", std::move(values));
}

ExprPtr identity_matrix(std::size_t size) {
    ExprList rows;
    for (std::size_t i = 0; i < size; ++i) {
        ExprList row;
        for (std::size_t j = 0; j < size; ++j)
            row.push_back(make_integer(i == j ? 1 : 0));
        rows.push_back(make_normal("List", std::move(row)));
    }
    return make_normal("List", std::move(rows));
}

ExprPtr determinant(const ExprPtr& value) {
    std::size_t rows = 0, columns = 0;
    if (!matrix(value, &rows, &columns) || rows != columns) return nullptr;
    if (rows == 1) return value->arg(0)->arg(0);
    ExprList terms;
    for (std::size_t column = 0; column < columns; ++column) {
        ExprList minor_rows;
        for (std::size_t row = 1; row < rows; ++row) {
            ExprList minor_row;
            for (std::size_t c = 0; c < columns; ++c)
                if (c != column) minor_row.push_back(value->arg(row)->arg(c));
            minor_rows.push_back(make_normal("List", std::move(minor_row)));
        }
        ExprPtr term = times(value->arg(0)->arg(column), determinant(
            make_normal("List", std::move(minor_rows))));
        if (column % 2 != 0) term = negate(term);
        terms.push_back(term);
    }
    return plus(std::move(terms));
}

ExprList rules(const ExprPtr& result) {
    ExprList all;
    if (!result->has_head("List")) return {};
    for (const auto& branch : result->args()) {
        if (!branch->has_head("List")) return {};
        for (const auto& rule : branch->args())
            if (!rule->has_head("Rule") || rule->size() != 2) return {};
            else all.push_back(rule);
    }
    return all;
}

bool verify_equations(const ExprPtr& equations, const ExprPtr& result) {
    const ExprList substitutions = rules(result);
    if (substitutions.empty()) return false;
    for (const auto& equation : equations->has_head("List")
             ? equations->args() : ExprList{equations}) {
        if (!equation->has_head("Equal") || equation->size() != 2) return false;
        ExprPtr lhs = replace_all(equation->arg(0), substitutions);
        ExprPtr rhs = replace_all(equation->arg(1), substitutions);
        if (!identity(lhs, rhs)) return false;
    }
    return true;
}

bool verify_integral(const ExprPtr& request, const ExprPtr& result) {
    if (numeric_mode && request->size() == 2 && request->arg(1)->has_head("List"))
        return numeric_definite_integral(request->arg(0), request->arg(1), result);
    if (request->size() != 2 || !request->arg(1)->is_symbol()) return false;
    try {
        return identity(differentiate(result, request->arg(1)), request->arg(0));
    } catch (...) {
        return false;
    }
}

bool verify_matrix_operation(const ExprPtr& request, const ExprPtr& result) {
    const std::string& head = request->head()->name();
    const ExprPtr& input = request->arg(0);
    if (head == "Det") {
        ExprPtr expected = determinant(input);
        return expected && identity(expected, result);
    }
    if (head == "Transpose") {
        std::size_t rows = 0, columns = 0;
        if (!matrix(input, &rows, &columns) || !matrix(result)) return false;
        ExprList values;
        for (std::size_t j = 0; j < columns; ++j) {
            ExprList row;
            for (std::size_t i = 0; i < rows; ++i) row.push_back(input->arg(i)->arg(j));
            values.push_back(make_normal("List", std::move(row)));
        }
        return same(make_normal("List", std::move(values)), result);
    }
    if (head == "Inverse") {
        ExprPtr product = matrix_product(input, result);
        return product && same(product, identity_matrix(input->size()));
    }
    if (head == "LinearSolve") {
        ExprPtr product = nullptr;
        if (result->has_head("List") && result->size() > 0 &&
            !result->arg(0)->has_head("List")) {
            ExprList column{result};
            product = matrix_product(input, make_normal("List", std::move(column)));
        }
        if (!product) return false;
        ExprList values;
        for (const auto& row : product->args()) values.push_back(row->arg(0));
        return same(make_normal("List", std::move(values)), request->arg(1));
    }
    return false;
}

bool verify(const ExprPtr& request, const ExprPtr& result) {
    if (!request || !result || !request->is_normal() || !request->head()->is_symbol())
        return false;
    const std::string& head = request->head()->name();
    if (head == "Integrate") return verify_integral(request, result);
    if (head == "Solve" || head == "DSolve")
        return request->size() > 0 && verify_equations(request->arg(0), result);
    if (head == "Factor" && request->size() == 1)
        return identity(expand(result), expand(request->arg(0)));
    if (head == "Det" || head == "Transpose" || head == "Inverse" ||
        head == "LinearSolve")
        return verify_matrix_operation(request, result);
    return false;
}

}  // namespace

bool verify_backend_result(const ExprPtr& request, const ExprPtr& result) {
    numeric_mode = false;
    return verify(request, result);
}

ResultStatus verification_status(const ExprPtr& request, const ExprPtr& result) {
    struct Reset {
        ~Reset() { numeric_mode = false; }
    } reset;
    numeric_mode = false;
    if (verify(request, result)) return ResultStatus::Verified;
    numeric_mode = true;
    if (verify(request, result)) return ResultStatus::Numeric;
    return ResultStatus::Unverified;
}

}  // namespace symats
