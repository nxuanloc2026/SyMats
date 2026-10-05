// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac_backend.h"

#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "giac.h"
#include "static_extern.h"

namespace symats {
namespace {

using GiacHead = const giac::unary_function_ptr* const*;
struct HeadMap { std::string_view name; GiacHead pointer; };

// Explicit names keep Symats' Tier 1 vocabulary independent of Giac's parser.
// Other heads use Giac's unevaluated function application (at_of).
#define GIAC_HEAD(symats_name, giac_name) {symats_name, &giac::at_##giac_name}
const HeadMap heads[] = {
    GIAC_HEAD("Plus", plus), GIAC_HEAD("Times", prod), GIAC_HEAD("Power", pow),
    GIAC_HEAD("Sin", sin), GIAC_HEAD("Cos", cos), GIAC_HEAD("Tan", tan),
    GIAC_HEAD("Cot", cot), GIAC_HEAD("Sec", sec), GIAC_HEAD("Csc", csc),
    GIAC_HEAD("ArcSin", asin), GIAC_HEAD("ArcCos", acos), GIAC_HEAD("ArcTan", atan),
    GIAC_HEAD("Sinh", sinh), GIAC_HEAD("Cosh", cosh), GIAC_HEAD("Tanh", tanh),
    GIAC_HEAD("Exp", exp), GIAC_HEAD("Log", ln), GIAC_HEAD("Abs", abs),
    GIAC_HEAD("Sqrt", sqrt), GIAC_HEAD("Factorial", factorial),
    GIAC_HEAD("Equal", equal), GIAC_HEAD("Unequal", different),
    GIAC_HEAD("Less", inferieur_strict), GIAC_HEAD("LessEqual", inferieur_egal),
    GIAC_HEAD("Greater", superieur_strict), GIAC_HEAD("GreaterEqual", superieur_egal),
    GIAC_HEAD("And", and), GIAC_HEAD("Or", ou), GIAC_HEAD("Not", not),
    GIAC_HEAD("Integrate", integrate), GIAC_HEAD("Limit", limit),
    GIAC_HEAD("Series", series), GIAC_HEAD("Solve", solve),
    GIAC_HEAD("Factor", factor), GIAC_HEAD("Simplify", simplify),
    GIAC_HEAD("DSolve", desolve), GIAC_HEAD("Det", det),
    GIAC_HEAD("Inverse", inverse), GIAC_HEAD("Rank", rank),
    GIAC_HEAD("Trace", trace), GIAC_HEAD("RREF", rref),
    GIAC_HEAD("Eigenvalues", eigenvalues), GIAC_HEAD("Eigenvectors", eigenvectors),
    GIAC_HEAD("CharacteristicPolynomial", charpoly),
};
#undef GIAC_HEAD

GiacHead find_head(std::string_view name) {
    for (const auto& entry : heads)
        if (entry.name == name) return entry.pointer;
    return nullptr;
}

std::string_view find_name(const giac::unary_function_ptr& head) {
    for (const auto& entry : heads)
        if (head == *entry.pointer) return entry.name;
    return {};
}

giac::gen to_giac(const ExprPtr& expr) {
    if (!expr) throw std::invalid_argument("null Expr");
    if (expr->is_integer()) {
        const auto small = expr->integer().to_int64();
        if (small && *small >= INT_MIN && *small <= INT_MAX)
            return giac::gen(static_cast<int>(*small));
        return giac::gen(mpz_class(expr->integer().to_string()));
    }
    if (expr->is_rational()) {
        const auto& r = expr->rational();
        return giac::gen(giac::fraction(to_giac(make_integer(r.num())),
                                         to_giac(make_integer(r.den()))));
    }
    if (expr->is_symbol()) {
        return giac::gen(giac::identificateur(expr->name()));
    }
    giac::vecteur args;
    args.reserve(expr->size());
    for (const auto& arg : expr->args()) args.push_back(to_giac(arg));
    if (expr->has_head("List")) return giac::gen(args);
    if (expr->head()->is_symbol()) {
        if (GiacHead head = find_head(expr->head()->name())) {
            const giac::gen leaf = args.size() == 1 ? args.front()
                : giac::gen(args, giac::_SEQ__VECT);
            return giac::gen(giac::symbolic(**head, leaf));
        }
    }
    const giac::gen arg = args.size() == 1 ? args.front()
        : giac::gen(args, giac::_SEQ__VECT);
    return giac::gen(giac::symbolic(*giac::at_of,
        giac::makesequence(to_giac(expr->head()), arg)));
}

std::optional<ExprPtr> from_giac(const giac::gen& value) {
    if (value.type == giac::_INT_)
        return make_integer(value.val);
    if (value.type == giac::_ZINT)
        return make_integer(Integer::from_string(value.print(giac::context0)));
    if (value.type == giac::_IDNT) {
        const std::string name = value.ref_IDNTptr()->name();
        return make_symbol(name);
    }
    if (value.type == giac::_FRAC) {
        const auto& f = *value.ref_FRACptr();
        auto n = from_giac(f.num), d = from_giac(f.den);
        if (!n || !d || !(*n)->is_integer() || !(*d)->is_integer()) return std::nullopt;
        return make_rational((*n)->integer(), (*d)->integer());
    }
    if (value.type == giac::_VECT) {
        ExprList args;
        for (const auto& item : *value.ref_VECTptr()) {
            auto converted = from_giac(item);
            if (!converted) return std::nullopt;
            args.push_back(*converted);
        }
        return make_normal("List", std::move(args));
    }
    if (value.type != giac::_SYMB) return std::nullopt;

    const auto& sym = *value.ref_SYMBptr();
    if (sym.sommet == giac::at_of) {
        if (sym.feuille.type != giac::_VECT || sym.feuille.ref_VECTptr()->size() != 2)
            return std::nullopt;
        const auto& pair = *sym.feuille.ref_VECTptr();
        auto head = from_giac(pair[0]);
        if (!head) return std::nullopt;
        ExprList args;
        if (pair[1].type == giac::_VECT && pair[1].subtype == giac::_SEQ__VECT) {
            for (const auto& item : *pair[1].ref_VECTptr()) {
                auto converted = from_giac(item);
                if (!converted) return std::nullopt;
                args.push_back(*converted);
            }
        } else {
            auto converted = from_giac(pair[1]);
            if (!converted) return std::nullopt;
            args.push_back(*converted);
        }
        return make_normal(*head, std::move(args));
    }

    const std::string_view name = find_name(sym.sommet);
    if (name.empty()) return std::nullopt;
    ExprList args;
    if (sym.feuille.type == giac::_VECT && sym.feuille.subtype == giac::_SEQ__VECT) {
        for (const auto& item : *sym.feuille.ref_VECTptr()) {
            auto converted = from_giac(item);
            if (!converted) return std::nullopt;
            args.push_back(*converted);
        }
    } else {
        auto converted = from_giac(sym.feuille);
        if (!converted) return std::nullopt;
        args.push_back(*converted);
    }
    if (name == "Plus") return plus(std::move(args));
    if (name == "Times") return times(std::move(args));
    if (name == "Power" && args.size() == 2) return power(args[0], args[1]);
    return make_normal(name, std::move(args));
}

}  // namespace

std::optional<ExprPtr> giac_roundtrip(const ExprPtr& expr) {
    try { return from_giac(to_giac(expr)); }
    catch (const std::exception&) { return std::nullopt; }
}

bool GiacBackend::supports(std::string_view head) const {
    constexpr std::string_view operations[] = {
        "Integrate", "Limit", "Series", "Solve", "Factor", "Simplify", "DSolve",
        "Det", "Inverse", "Rank", "Trace", "RREF", "Eigenvalues",
        "Eigenvectors", "CharacteristicPolynomial"
    };
    for (const auto& operation : operations)
        if (head == operation) return true;
    return false;
}

std::optional<BackendResult> GiacBackend::evaluate(const ExprPtr& expr) {
    if (!expr || !expr->is_normal() || !expr->head()->is_symbol() ||
        !supports(expr->head()->name())) return std::nullopt;
    try {
        std::cerr << "eval before to\n";
        const giac::gen input = to_giac(expr);
        std::cerr << "eval after to\n";
        giac::context context;
        std::cerr << "eval after context\n";
        std::cerr << "input: " << input.print(&context) << "\n";
        std::cerr << "arg: " << to_giac(expr->arg(0)).print(&context) << "\n";
        const giac::gen output = expr->has_head("Factor")
            ? giac::_factor(to_giac(expr->arg(0)), &context)
            : input.eval(1, &context);
        std::cerr << "eval after eval\n";
        if (output == input) return std::nullopt;
        auto converted = from_giac(output);
        std::cerr << "eval after from\n";
        if (!converted) return std::nullopt;
        return BackendResult{*converted, ResultStatus::Unverified, "giac"};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace symats
