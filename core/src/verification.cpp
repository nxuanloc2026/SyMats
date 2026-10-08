// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/verification.h"

#include "symats/calculus.h"
#include "symats/eval.h"
#include "symats/pattern.h"

namespace symats {
namespace {

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
    return zero(subtract(lhs, rhs));
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
        return equal(make_normal("List", std::move(values)), result);
    }
    if (head == "Inverse") {
        ExprPtr product = matrix_product(input, result);
        return product && equal(product, identity_matrix(input->size()));
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
        return equal(make_normal("List", std::move(values)), request->arg(1));
    }
    return false;
}

}  // namespace

bool verify_backend_result(const ExprPtr& request, const ExprPtr& result) {
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

}  // namespace symats
