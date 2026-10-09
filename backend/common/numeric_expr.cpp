// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "numeric_expr.h"

#include <cmath>

#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/zeta.hpp>

namespace symats::numeric {
namespace {
using Fn1 = double (*)(double);
using Fn2 = double (*)(double, double);

const std::map<std::string, Fn1, std::less<>>& unary_functions() {
    static const std::map<std::string, Fn1, std::less<>> table = {
        {"Sin", [](double x) { return std::sin(x); }},
        {"Cos", [](double x) { return std::cos(x); }},
        {"Tan", [](double x) { return std::tan(x); }},
        {"Cot", [](double x) { return 1.0 / std::tan(x); }},
        {"Sec", [](double x) { return 1.0 / std::cos(x); }},
        {"Csc", [](double x) { return 1.0 / std::sin(x); }},
        {"ArcSin", [](double x) { return std::asin(x); }},
        {"ArcCos", [](double x) { return std::acos(x); }},
        {"ArcTan", [](double x) { return std::atan(x); }},
        {"Sinh", [](double x) { return std::sinh(x); }},
        {"Cosh", [](double x) { return std::cosh(x); }},
        {"Tanh", [](double x) { return std::tanh(x); }},
        {"ArcSinh", [](double x) { return std::asinh(x); }},
        {"ArcCosh", [](double x) { return std::acosh(x); }},
        {"ArcTanh", [](double x) { return std::atanh(x); }},
        {"Exp", [](double x) { return std::exp(x); }},
        {"Log", [](double x) { return std::log(x); }},
        {"Sqrt", [](double x) { return std::sqrt(x); }},
        {"Abs", [](double x) { return std::abs(x); }},
        {"Gamma", [](double x) { return boost::math::tgamma(x); }},
        {"LogGamma", [](double x) { return boost::math::lgamma(x); }},
        {"Erf", [](double x) { return boost::math::erf(x); }},
        {"Erfc", [](double x) { return boost::math::erfc(x); }},
        {"Zeta", [](double x) { return boost::math::zeta(x); }},
    };
    return table;
}

const std::map<std::string, Fn2, std::less<>>& binary_functions() {
    static const std::map<std::string, Fn2, std::less<>> table = {
        {"Log", [](double b, double x) { return std::log(x) / std::log(b); }},
        {"ArcTan", [](double x, double y) { return std::atan2(y, x); }},
        {"Gamma", [](double a, double x) { return boost::math::tgamma(a, x); }},
        {"Beta", [](double a, double b) { return boost::math::beta(a, b); }},
        {"BesselJ", [](double n, double x) { return boost::math::cyl_bessel_j(n, x); }},
        {"BesselY", [](double n, double x) { return boost::math::cyl_neumann(n, x); }},
        {"BesselI", [](double n, double x) { return boost::math::cyl_bessel_i(n, x); }},
        {"BesselK", [](double n, double x) { return boost::math::cyl_bessel_k(n, x); }},
    };
    return table;
}
}  // namespace

double Compiled::eval(const double* slots) const {
    switch (op) {
    case Op::Const: return value;
    case Op::Slot: return slots[index];
    case Op::Plus: {
        double r = 0.0;
        for (const auto& a : args) r += a.eval(slots);
        return r;
    }
    case Op::Times: {
        double r = 1.0;
        for (const auto& a : args) r *= a.eval(slots);
        return r;
    }
    case Op::Power: return std::pow(args[0].eval(slots), args[1].eval(slots));
    case Op::Call1: return f1(args[0].eval(slots));
    case Op::Call2: return f2(args[0].eval(slots), args[1].eval(slots));
    }
    return 0.0;
}

Compiled compile(const ExprPtr& e, const Slots& slots) {
    Compiled n;
    if (e->is_number()) {
        n.value = e->number().to_double();
        return n;
    }
    if (e->is_symbol()) {
        const auto& s = e->name();
        if (s == "Pi") n.value = 3.14159265358979323846;
        else if (s == "E") n.value = 2.71828182845904523536;
        else if (auto it = slots.find(s); it != slots.end()) {
            n.op = Compiled::Op::Slot;
            n.index = it->second;
        } else throw Unsupported();
        return n;
    }
    if (!e->head()->is_symbol()) throw Unsupported();
    const auto& head = e->head()->name();
    for (const auto& a : e->args()) n.args.push_back(compile(a, slots));
    if (head == "Plus") n.op = Compiled::Op::Plus;
    else if (head == "Times") n.op = Compiled::Op::Times;
    else if (head == "Power" && e->size() == 2) n.op = Compiled::Op::Power;
    else if (auto f = unary_functions().find(head); e->size() == 1 && f != unary_functions().end()) {
        n.op = Compiled::Op::Call1;
        n.f1 = f->second;
    } else if (auto g = binary_functions().find(head); e->size() == 2 && g != binary_functions().end()) {
        n.op = Compiled::Op::Call2;
        n.f2 = g->second;
    } else throw Unsupported();
    return n;
}

double constant(const ExprPtr& e) { return compile(e, Slots{}).eval(nullptr); }

ExprPtr decimal(double value) {
    if (!std::isfinite(value)) throw NonFinite();
    if (value == 0.0) return make_integer(0);
    const int exponent = static_cast<int>(std::floor(std::log10(std::abs(value))));
    int shift = 11 - exponent;
    double scaled = value;
    for (; shift > 300; shift -= 300) scaled *= 1e300;
    const long long mantissa = std::llround(scaled * std::pow(10.0, shift));
    if (shift >= 0) return make_rational(Integer(mantissa), Integer::pow(10, static_cast<unsigned>(shift)));
    return make_integer(Integer(mantissa) * Integer::pow(10, static_cast<unsigned>(-shift)));
}

}  // namespace symats::numeric
