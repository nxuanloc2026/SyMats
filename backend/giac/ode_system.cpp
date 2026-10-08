// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "conversion.h"

#include <stdexcept>
#include "static_extern.h"

namespace symats::giac_detail {
// Adapt first-order constant-coefficient systems to Giac's Laplace and linear
// solvers. L[y'] = s L[y] - y[0] (see Boyce & DiPrima, Elementary Differential
// Equations, chapter on the Laplace transform). Giac performs all algebra.
giac::gen solve_system(const ExprPtr& expr, giac::context* context) {
    const auto& variable = expr->arg(2);
    const auto& dependents = expr->arg(1)->args();
    const auto n = dependents.size();
    auto require = [](bool ok) {
        if (!ok) throw std::invalid_argument("unsupported differential system");
    };
    require(n > 0 && expr->arg(0)->has_head("List"));
    giac::vecteur functions, states, rates, transforms, initial, originals, replacements;
    const auto x = to_giac(variable);
    const giac::gen s(giac::identificateur("symats_internal_laplace"));
    for (std::size_t j = 0; j < n; ++j) {
        const auto& dependent = dependents[j];
        const auto function = dependent->is_symbol() ? make_normal(dependent,{variable}) : dependent;
        require(function->is_normal() && function->head()->is_symbol() &&
                function->size()==1 && equal(function->arg(0),variable));
        const auto y = to_giac(function);
        for (const auto& previous : functions) require(previous != y);
        functions.push_back(y);
        states.push_back(giac::gen(giac::identificateur("symats_internal_y"+std::to_string(j))));
        rates.push_back(giac::gen(giac::identificateur("symats_internal_dy"+std::to_string(j))));
        transforms.push_back(giac::gen(giac::identificateur("symats_internal_Y"+std::to_string(j))));
        initial.push_back(giac::diffeq_constante(static_cast<int>(j),context));
        originals.push_back(to_giac(make_normal("D",{function,variable})));
        replacements.push_back(rates.back());
        originals.push_back(to_giac(make_normal("D",{function,make_normal("List",{variable,make_integer(1)})})));
        replacements.push_back(rates.back());
    }
    for (const auto& function : functions) originals.push_back(function);
    for (const auto& state : states) replacements.push_back(state);
    giac::vecteur unknowns = rates;
    for (const auto& state : states) unknowns.push_back(state);
    const giac::vecteur zeros(unknowns.size(),giac::gen(0));
    giac::vecteur equations;
    std::vector<bool> prescribed(n,false);
    for (const auto& equation : expr->arg(0)->args()) {
        require(equation->has_head("Equal") && equation->size()==2);
        bool condition = false;
        for (std::size_t j = 0; j < n; ++j) {
            const auto& dependent = dependents[j];
            const auto name = dependent->is_symbol() ? dependent : dependent->head();
            if (!equal(equation->arg(0),make_normal(name,{make_integer(0)}))) continue;
            require(!prescribed[j]);
            const auto value = to_giac(equation->arg(1));
            require(giac::is_zero(giac::derive(value,x,context)));
            initial[j] = value;
            prescribed[j] = condition = true;
        }
        if (!condition) equations.push_back(giac::subst(
            to_giac(subtract(equation->arg(0),equation->arg(1)),true,variable),
            originals,replacements,true,context));
    }
    require(equations.size()==n);
    giac::vecteur transformed;
    for (const auto& equation : equations) {
        const auto residual = giac::normal(giac::subst(equation,unknowns,zeros,false,context),context);
        auto remainder = equation-residual;
        giac::gen row = giac::_laplace(giac::makesequence(residual,x,s),context);
        for (std::size_t j = 0; j < n; ++j) {
            const auto a = giac::normal(giac::derive(equation,rates[j],context),context);
            const auto b = giac::normal(giac::derive(equation,states[j],context),context);
            for (const auto& coefficient : giac::makevecteur(a,b)) {
                require(giac::is_zero(giac::derive(coefficient,x,context)));
                require(giac::subst(coefficient,unknowns,zeros,false,context)==coefficient);
            }
            remainder -= a*rates[j]+b*states[j];
            row += (a*s+b)*transforms[j]-a*initial[j];
        }
        require(giac::is_zero(giac::normal(remainder,context)));
        transformed.push_back(row);
    }
    const auto solved = giac::_linsolve(giac::makesequence(
        giac::gen(transformed),giac::gen(transforms)),context);
    require(solved.type==giac::_VECT && solved.ref_VECTptr()->size()==n);
    giac::vecteur result;
    for (const auto& value : *solved.ref_VECTptr())
        result.push_back(giac::_ilaplace(giac::makesequence(value,s,x),context));
    return giac::gen(result);
}
}  // namespace symats::giac_detail
