// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "conversion.h"

#include <climits>
#include <stdexcept>
#include "static_extern.h"

namespace symats::giac_detail {
namespace {
struct HeadMap { std::string_view name; GiacHead pointer; };
#define GIAC_HEAD(name, head) {name, &giac::at_##head}
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
    GIAC_HEAD("Together", normal), GIAC_HEAD("Apart", partfrac),
    GIAC_HEAD("DSolve", desolve), GIAC_HEAD("Det", det),
    GIAC_HEAD("Inverse", inverse), GIAC_HEAD("Rank", rank),
    GIAC_HEAD("Trace", trace), GIAC_HEAD("RowReduce", rref),
    GIAC_HEAD("Eigenvalues", eigenvalues), GIAC_HEAD("Eigenvectors", eigenvectors),
    GIAC_HEAD("CharPoly", charpoly), GIAC_HEAD("Transpose", tran),
    GIAC_HEAD("LinearSolve", linsolve),
};
#undef GIAC_HEAD

// Encoding every user name prevents collisions with Giac's lowercase built-ins
// and generated constants. No user text is parsed or executed by Giac.
constexpr std::string_view prefix = "symats_";
std::string encode(std::string_view name) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(prefix);
    for (unsigned char c : name) {
        result += digits[c >> 4];
        result += digits[c & 15];
    }
    return result;
}

std::optional<std::string> decode(std::string_view name) {
    if (!name.starts_with(prefix)) return std::nullopt;
    name.remove_prefix(prefix.size());
    if (name.size() % 2) return std::nullopt;
    std::string result;
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < name.size(); i += 2) {
        const int high = digit(name[i]), low = digit(name[i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        result += static_cast<char>(high * 16 + low);
    }
    return result;
}

giac::gen application(const giac::unary_function_ptr& head, const giac::gen& args) {
    return giac::gen(giac::symbolic(head, args));
}
}  // namespace

GiacHead find_head(std::string_view name) {
    for (const auto& entry : heads)
        if (entry.name == name) return entry.pointer;
    return nullptr;
}

giac::gen to_giac(const ExprPtr& expr, bool for_evaluation, const ExprPtr& ode_variable) {
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
        if (for_evaluation) {
            if (expr->name() == "Pi") return giac::cst_pi;
            if (expr->name() == "I") return giac::cst_i;
            if (expr->name() == "Infinity") return giac::plus_inf;
            if (expr->name() == "E") return application(*giac::at_exp, giac::gen(1));
            if (expr->name() == "True" || expr->name() == "False") {
                giac::gen value(expr->name() == "True" ? 1 : 0);
                value.subtype = giac::_INT_BOOLEAN;
                return value;
            }
        }
        return giac::gen(giac::identificateur(encode(expr->name())));
    }
    giac::vecteur args;
    for (const auto& arg : expr->args()) args.push_back(to_giac(arg, for_evaluation, ode_variable));
    if (expr->has_head("List")) return giac::gen(args);
    if (for_evaluation) {
        if (expr->has_head("Log") && args.size() == 2)
            return application(*giac::at_ln, args[1]) / application(*giac::at_ln, args[0]);
        if (expr->has_head("Root") && args.size() == 2)
            return application(*giac::at_pow, giac::makesequence(args[0], giac::gen(1)/args[1]));
        if (expr->has_head("D") && args.size() >= 2) {
            giac::gen value = args[0];
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (expr->arg(i)->has_head("List") && expr->arg(i)->size() == 2)
                    value = application(*giac::at_derive, giac::makesequence(
                        value, args[i].ref_VECTptr()->at(0), args[i].ref_VECTptr()->at(1)));
                else value = application(*giac::at_derive, giac::makesequence(value, args[i]));
            }
            return value;
        }
        const auto& head = expr->head();
        if (args.size() == 1 && head->is_normal() && head->size() == 1 &&
            head->head()->has_head("Derivative") && head->head()->size() == 1) {
            const auto variable = ode_variable ? ode_variable : expr->arg(0);
            if (!variable->is_symbol()) throw std::invalid_argument("derivative needs a variable");
            const auto function = to_giac(head->arg(0));
            if (equal(expr->arg(0), variable))
                return application(*giac::at_derive, giac::makesequence(
                    application(*giac::at_of, giac::makesequence(function, args[0])),
                    to_giac(variable), to_giac(head->head()->arg(0))));
            const auto derivative = application(*giac::at_derive, giac::makesequence(
                function, to_giac(variable), to_giac(head->head()->arg(0))));
            return application(*giac::at_of, giac::makesequence(derivative, args[0]));
        }
    }
    const giac::gen leaf = args.size() == 1 ? args.front() : giac::gen(args, giac::_SEQ__VECT);
    if (expr->head()->is_symbol())
        if (GiacHead head = find_head(expr->head()->name())) return application(**head, leaf);
    return application(*giac::at_of,
        giac::makesequence(to_giac(expr->head(), for_evaluation, ode_variable), leaf));
}

std::optional<ExprPtr> from_giac(const giac::gen& value, const std::set<std::string>& reserved) {
    if (giac::is_undef(value)) return std::nullopt;
    if (value == giac::plus_inf) return make_symbol("Infinity");
    if (value == giac::minus_inf) return negate(make_symbol("Infinity"));
    if (value.type == giac::_INT_) {
        if (value.subtype == giac::_INT_BOOLEAN) return make_symbol(value.val ? "True" : "False");
        return make_integer(value.val);
    }
    if (value.type == giac::_ZINT)
        return make_integer(Integer::from_string(value.print(giac::context0)));
    if (value.type == giac::_IDNT) {
        const std::string name = value.ref_IDNTptr()->name();
        if (auto decoded = decode(name)) return make_symbol(*decoded);
        if (name == "pi") return make_symbol("Pi");
        if (name.starts_with("c_") && name.size() > 2 &&
            name.find_first_not_of("0123456789", 2) == std::string::npos) {
            auto remaining = std::stoull(name.substr(2));
            for (std::size_t i = 1; ; ++i) {
                const auto candidate = "C" + std::to_string(i);
                if (!reserved.contains(candidate)) {
                    if (remaining == 0) return make_symbol(candidate);
                    --remaining;
                }
            }
        }
        return std::nullopt;
    }
    if (value.type == giac::_FRAC) {
        const auto& f = *value.ref_FRACptr();
        auto n = from_giac(f.num,reserved), d = from_giac(f.den,reserved);
        if (!n || !d) return std::nullopt;
        return divide(*n, *d);
    }
    if (value.type == giac::_CPLX) {
        auto real = from_giac(value.ref_CPLXptr()[0],reserved), imaginary = from_giac(value.ref_CPLXptr()[1],reserved);
        if (!real || !imaginary) return std::nullopt;
        return plus({*real, times({make_symbol("I"), *imaginary})});
    }
    if (value.type == giac::_VECT) {
        ExprList args;
        for (const auto& item : *value.ref_VECTptr()) {
            auto converted = from_giac(item,reserved);
            if (!converted) return std::nullopt;
            args.push_back(*converted);
        }
        return make_normal("List", std::move(args));
    }
    // Approximate and unsupported Giac types decline; they must not become exact Expr numbers.
    if (value.type != giac::_SYMB) return std::nullopt;
    const auto& sym = *value.ref_SYMBptr();
    if (sym.sommet == giac::at_inv || sym.sommet == giac::at_neg) {
        auto arg = from_giac(sym.feuille,reserved);
        if (!arg) return std::nullopt;
        return sym.sommet == giac::at_inv ? power(*arg, make_integer(-1)) : negate(*arg);
    }
    ExprPtr head;
    giac::gen leaf = sym.feuille;
    if (sym.sommet == giac::at_of) {
        if (leaf.type != giac::_VECT || leaf.ref_VECTptr()->size() != 2) return std::nullopt;
        auto converted = from_giac(leaf.ref_VECTptr()->at(0),reserved);
        if (!converted) return std::nullopt;
        head = *converted;
        const giac::gen arguments = leaf.ref_VECTptr()->at(1);
        leaf = arguments;
    } else {
        for (const auto& entry : heads)
            if (sym.sommet == *entry.pointer) { head = make_symbol(std::string(entry.name)); break; }
        if (sym.sommet == giac::at_derive) head = make_symbol("D");
        if (!head) return std::nullopt;
    }
    ExprList args;
    const giac::vecteur leaves = leaf.type == giac::_VECT && leaf.subtype == giac::_SEQ__VECT
        ? *leaf.ref_VECTptr() : giac::makevecteur(leaf);
    for (const auto& item : leaves) {
        auto converted = from_giac(item,reserved);
        if (!converted) return std::nullopt;
        args.push_back(*converted);
    }
    if (head->is_symbol()) {
        if (head->name() == "Plus") return plus(std::move(args));
        if (head->name() == "Times") return times(std::move(args));
        if (head->name() == "Power" && args.size() == 2) return power(args[0], args[1]);
        if (head->name() == "D" && args.size() == 3)
            return make_normal("D", {args[0], make_normal("List", {args[1], args[2]})});
    }
    return make_normal(head, std::move(args));
}

giac::gen sequence(const ExprList& args, const ExprPtr& ode_variable) {
    giac::vecteur values;
    for (const auto& arg : args) values.push_back(to_giac(arg, true, ode_variable));
    return giac::gen(values, giac::_SEQ__VECT);
}
}  // namespace symats::giac_detail
