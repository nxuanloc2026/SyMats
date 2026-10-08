// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac_backend.h"
#include "conversion.h"

#include <mutex>
#include <sstream>
#include <climits>
#include <stdexcept>
#include "static_extern.h"
#include "symats/verification.h"

namespace symats {
namespace {
using namespace giac_detail;
std::mutex giac_mutex;  // Giac also uses process-wide caches outside its context.

struct Operation { std::string_view name; std::size_t minimum, maximum; };
constexpr Operation operations[] = {
    {"Integrate", 2, 32}, {"Limit", 2, 2}, {"Series", 2, 2},
    {"Solve", 2, 2}, {"DSolve", 3, 3}, {"Factor", 1, 1}, {"Simplify", 1, 1},
    {"Together", 1, 1}, {"Apart", 1, 2}, {"Det", 1, 1}, {"Inverse", 1, 1},
    {"Rank", 1, 1}, {"Trace", 1, 1}, {"RowReduce", 1, 1},
    {"Eigenvalues", 1, 1}, {"Eigenvectors", 1, 1}, {"CharPoly", 2, 2},
    {"Transpose", 1, 1}, {"LinearSolve", 2, 2}, {"MatrixExp", 1, 1}, {"Dot", 2, 2},
};

const Operation* operation(std::string_view head) {
    for (const auto& item : operations) if (item.name == head) return &item;
    return nullptr;
}

void require(bool condition) {
    if (!condition) throw std::invalid_argument("unsupported Giac argument shape");
}

void symbol_names(const ExprPtr& expr, std::set<std::string>& names) {
    if (expr->is_symbol()) names.insert(expr->name());
    if (!expr->is_normal()) return;
    symbol_names(expr->head(),names);
    for (const auto& arg : expr->args()) symbol_names(arg,names);
}

bool unevaluated(const ExprPtr& expr) {
    if (!expr->is_normal()) return false;
    for (const auto name : {"Integrate","Limit","Series","Solve","DSolve"})
        if (expr->has_head(name)) return true;
    for (const auto& arg : expr->args()) if (unevaluated(arg)) return true;
    return false;
}

bool is_matrix(const ExprPtr& expr, bool square = false) {
    if (!expr->has_head("List") || expr->size() == 0) return false;
    const auto columns = expr->arg(0)->size();
    if (columns == 0 || (square && columns != expr->size())) return false;
    for (const auto& row : expr->args())
        if (!row->has_head("List") || row->size() != columns) return false;
    return true;
}

ExprList variables(const ExprPtr& expr) {
    const ExprList result = expr->has_head("List") ? expr->args() : ExprList{expr};
    require(!result.empty());
    for (std::size_t i = 0; i < result.size(); ++i) {
        require(result[i]->is_symbol());
        for (std::size_t j = 0; j < i; ++j) require(!equal(result[i], result[j]));
    }
    return result;
}

ExprPtr function_symbol(const ExprPtr& expr, const ExprPtr& variable) {
    if (expr->is_symbol()) return expr;
    require(expr->is_normal() && expr->head()->is_symbol() && expr->size() == 1 &&
            equal(expr->arg(0), variable));
    return expr->head();
}

giac::gen invoke(const ExprPtr& expr, giac::context* context) {
    const auto& name = expr->head()->name();
    const auto& args = expr->args();
    if (name == "Integrate") {
        giac::gen value = to_giac(args[0]);
        // Bounds are ordered inner integral first, as in EXPR_SPEC.
        for (std::size_t i = 1; i < args.size(); ++i) {
            const auto& bound = args[i];
            giac::vecteur call = giac::makevecteur(value);
            if (bound->is_symbol()) call.push_back(to_giac(bound));
            else {
                require(bound->has_head("List") && bound->size() == 3 && bound->arg(0)->is_symbol());
                for (const auto& item : bound->args()) call.push_back(to_giac(item));
            }
            value = giac::_integrate(giac::gen(call, giac::_SEQ__VECT), context);
        }
        return value;
    }
    if (name == "Limit") {
        require(args[1]->has_head("Rule") && args[1]->size() == 2 && args[1]->arg(0)->is_symbol());
        return giac::_limit(sequence({args[0], args[1]->arg(0), args[1]->arg(1)}), context);
    }
    if (name == "Series") {
        require(args[1]->has_head("List") && args[1]->size() == 3 && args[1]->arg(0)->is_symbol());
        const auto& order = args[1]->arg(2);
        require(order->is_integer() && order->integer().sign() >= 0 &&
                order->integer().to_int64().has_value() && *order->integer().to_int64() <= INT_MAX);
        giac::vecteur call = *sequence({args[0], args[1]->arg(0), args[1]->arg(1), order}).ref_VECTptr();
        giac::gen polynomial_option(giac::_POLY1__VECT);
        polynomial_option.subtype = giac::_INT_MAPLECONVERSION;
        call.push_back(polynomial_option);
        return giac::_series(giac::gen(call, giac::_SEQ__VECT), context);
    }
    if (name == "Solve") {
        variables(args[1]);
        return giac::_solve(sequence(args), context);
    }
    if (name == "DSolve") {
        require(args[2]->is_symbol());
        if (args[1]->has_head("List")) return solve_system(expr,context);
        const ExprList dependents = args[1]->has_head("List") ? args[1]->args() : ExprList{args[1]};
        require(!dependents.empty());
        ExprList names;
        for (const auto& dependent : dependents) names.push_back(function_symbol(dependent, args[2]));
        const auto funcs = args[1]->has_head("List") ? make_normal("List", names) : names[0];
        return giac::_desolve(sequence({args[0], args[2], funcs}, args[2]), context);
    }
    const bool square = name == "Det" || name == "Inverse" || name == "Trace" ||
        name == "Eigenvalues" || name == "Eigenvectors" || name == "CharPoly" || name == "MatrixExp";
    if (square || name == "Rank" || name == "RowReduce" || name == "Transpose" || name == "LinearSolve")
        require(is_matrix(args[0], square));
    if (name == "CharPoly") require(args[1]->is_symbol());
    if (name == "LinearSolve")
        require(args[1]->has_head("List") && args[1]->size() == args[0]->size());
    if (name == "MatrixExp")
        return giac::gen(giac::analytic_apply(giac::at_exp, *to_giac(args[0]).ref_VECTptr(), context));
    if (name == "Dot") {
        for (const auto& arg : args) {
            require(arg->has_head("List") && arg->size()>0);
            const bool matrix = arg->arg(0)->has_head("List");
            if (matrix) require(is_matrix(arg));
            else for (const auto& item : arg->args()) require(!item->has_head("List"));
        }
        const auto columns = is_matrix(args[0]) ? args[0]->arg(0)->size() : args[0]->size();
        require(columns == args[1]->size());
        return to_giac(args[0]) * to_giac(args[1]);
    }
    const auto head = find_head(name);
    require(head != nullptr);
    const giac::gen call = args.size() == 1 ? to_giac(args[0]) : sequence(args);
    giac::gen result = (**head)(call, context);
    // Giac supplies eigenvectors as columns; Symats returns a list of vectors.
    if (name == "Eigenvectors" && giac::ckmatrix(result)) result = giac::_tran(result, context);
    return result;
}

std::optional<ExprPtr> solution_rules(const ExprPtr& output, const ExprList& targets, bool system) {
    ExprList branches;
    if (!output->has_head("List")) return std::nullopt;
    if (!system) {
        for (const auto& root : output->args()) {
            if (root->has_head("Equal")) return std::nullopt;
            branches.push_back(make_normal("List", {make_normal("Rule", {targets[0], root})}));
        }
    } else {
        for (const auto& values : output->args()) {
            if (!values->has_head("List") || values->size() != targets.size()) return std::nullopt;
            ExprList rules;
            for (std::size_t i = 0; i < targets.size(); ++i) {
                if (values->arg(i)->has_head("Equal")) return std::nullopt;
                rules.push_back(make_normal("Rule", {targets[i], values->arg(i)}));
            }
            branches.push_back(make_normal("List", std::move(rules)));
        }
    }
    return make_normal("List", std::move(branches));
}
}  // namespace

std::optional<ExprPtr> giac_roundtrip(const ExprPtr& expr) {
    const std::lock_guard lock(giac_mutex);
    try { return from_giac(to_giac(expr, false)); }
    catch (const std::exception&) { return std::nullopt; }
}

bool GiacBackend::supports(std::string_view head) const { return operation(head) != nullptr; }

std::optional<BackendResult> GiacBackend::evaluate(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || !expr->head()->is_symbol()) return std::nullopt;
    const auto op = operation(expr->head()->name());
    if (!op || expr->size() < op->minimum || expr->size() > op->maximum) return std::nullopt;
    const std::lock_guard lock(giac_mutex);
    try {
        giac::context context;
        std::ostringstream diagnostics;
        giac::logptr(&diagnostics,&context);
        giac::approx_mode(false, &context);
        giac::angle_radian(true, &context);
        const giac::gen output = invoke(expr, &context);
        if (output == to_giac(expr, true, expr->has_head("DSolve") ? expr->arg(2) : ExprPtr{})) return std::nullopt;
        std::set<std::string> reserved;
        symbol_names(expr,reserved);
        auto converted = from_giac(output,reserved);
        if (!converted || unevaluated(*converted)) return std::nullopt;
        if (expr->has_head("Series"))
            converted = make_normal("SeriesData", {*converted, expr->arg(1)});
        if (expr->has_head("Solve"))
            converted = solution_rules(*converted, variables(expr->arg(1)), expr->arg(1)->has_head("List"));
        if (expr->has_head("DSolve")) {
            const bool system = expr->arg(1)->has_head("List");
            ExprList targets = system ? expr->arg(1)->args() : ExprList{expr->arg(1)};
            for (auto& target : targets)
                if (target->is_symbol()) target = make_normal(target, {expr->arg(2)});
            ExprPtr values = *converted;
            if (system || !values->has_head("List")) values = make_normal("List", {values});
            converted = solution_rules(values, targets, system);
        }
        if (!converted) return std::nullopt;
        const ResultStatus status = verify_backend_result(expr, *converted)
            ? ResultStatus::Verified : ResultStatus::Unverified;
        return BackendResult{*converted, status, "giac"};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}
}  // namespace symats
