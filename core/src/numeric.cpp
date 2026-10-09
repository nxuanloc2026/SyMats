// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/numeric.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace symats::numeric {

const Functions& elementary_functions() {
    static const Functions table = {
        {
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
        },
        {
            {"Log", [](double b, double x) { return std::log(x) / std::log(b); }},
            {"ArcTan", [](double x, double y) { return std::atan2(y, x); }},
        },
    };
    return table;
}

namespace {
Functions& mutable_default_functions() {
    static Functions table = elementary_functions();
    return table;
}
}  // namespace

const Functions& default_functions() { return mutable_default_functions(); }

void register_functions(const Functions& extra) {
    auto& table = mutable_default_functions();
    for (const auto& [name, fn] : extra.unary) table.unary[name] = fn;
    for (const auto& [name, fn] : extra.binary) table.binary[name] = fn;
}

double Samples::operator()(double at) const {
    const std::size_t n = t.size();
    if (n == 0 || std::isnan(at)) return std::numeric_limits<double>::quiet_NaN();
    const double slack = 1e-12 * (1.0 + std::abs(t.back() - t.front()));
    if (at < t.front() - slack || at > t.back() + slack) return std::numeric_limits<double>::quiet_NaN();
    if (n == 1) return y[0];
    // Interval [t[i], t[i+1]] containing `at`, then the 4 nearest samples around it.
    const std::size_t i = std::min<std::size_t>(
        n - 2, static_cast<std::size_t>(std::max<std::ptrdiff_t>(
                   0, std::upper_bound(t.begin(), t.end(), at) - t.begin() - 1)));
    const std::size_t lo = n < 4 ? 0 : std::min(n - 4, i > 0 ? i - 1 : 0);
    const std::size_t hi = std::min(n, lo + 4);
    double sum = 0.0;
    for (std::size_t j = lo; j < hi; ++j) {
        double w = 1.0;
        for (std::size_t k = lo; k < hi; ++k)
            if (k != j) w *= (at - t[k]) / (t[j] - t[k]);
        sum += w * y[j];
    }
    return sum;
}

std::shared_ptr<const Samples> interpolating_samples(const ExprPtr& f) {
    if (!f->has_head("InterpolatingFunction") || f->size() != 2 || !f->arg(1)->has_head("List"))
        throw Unsupported();
    auto samples = std::make_shared<Samples>();
    for (const auto& point : f->arg(1)->args()) {
        if (!point->has_head("List") || point->size() != 2 || !point->arg(0)->is_number() ||
            !point->arg(1)->is_number()) throw Unsupported();
        const double t = point->arg(0)->number().to_double();
        if (!samples->t.empty() && !(t > samples->t.back())) throw Unsupported();
        samples->t.push_back(t);
        samples->y.push_back(point->arg(1)->number().to_double());
    }
    if (samples->t.empty()) throw Unsupported();
    return samples;
}

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
    case Op::Interpolate: return (*samples)(args[0].eval(slots));
    }
    return 0.0;
}

Compiled compile(const ExprPtr& e, const Slots& slots, const Functions& functions) {
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
    if (e->head()->has_head("InterpolatingFunction") && e->size() == 1) {
        n.op = Compiled::Op::Interpolate;
        n.samples = interpolating_samples(e->head());
        n.args.push_back(compile(e->arg(0), slots, functions));
        return n;
    }
    if (!e->head()->is_symbol()) throw Unsupported();
    const auto& head = e->head()->name();
    for (const auto& a : e->args()) n.args.push_back(compile(a, slots, functions));
    if (head == "Plus") n.op = Compiled::Op::Plus;
    else if (head == "Times") n.op = Compiled::Op::Times;
    else if (head == "Power" && e->size() == 2) n.op = Compiled::Op::Power;
    else if (auto f = functions.unary.find(head); e->size() == 1 && f != functions.unary.end()) {
        n.op = Compiled::Op::Call1;
        n.f1 = f->second;
    } else if (auto g = functions.binary.find(head); e->size() == 2 && g != functions.binary.end()) {
        n.op = Compiled::Op::Call2;
        n.f2 = g->second;
    } else throw Unsupported();
    return n;
}

double constant(const ExprPtr& e, const Functions& functions) {
    return compile(e, Slots{}, functions).eval(nullptr);
}

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
