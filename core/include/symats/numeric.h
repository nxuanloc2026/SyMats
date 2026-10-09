// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Fast double-precision evaluation of Expr trees, for numeric verification in core/
// and for numeric backends (NDSolve, NIntegrate). An expression is compiled once
// against named slots (its free variables) and then evaluated many times.
// core/ knows the elementary functions (<cmath>); backends may pass a larger
// function table (e.g. Boost.Math special functions, see backend/common).
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "symats/expr.h"

namespace symats::numeric {

struct Unsupported : std::runtime_error {
    Unsupported() : std::runtime_error("expression cannot be evaluated numerically") {}
};

struct NonFinite : std::runtime_error {
    NonFinite() : std::runtime_error("numeric evaluation produced a non-finite value") {}
};

// Symbol name -> index into the slot array passed to Compiled::eval.
using Slots = std::map<std::string, std::size_t, std::less<>>;

using Fn1 = double (*)(double);
using Fn2 = double (*)(double, double);

// Numeric implementations of heads, by argument count.
struct Functions {
    std::map<std::string, Fn1, std::less<>> unary;
    std::map<std::string, Fn2, std::less<>> binary;
};

// Sin ... ArcTanh, Exp, Log (1 and 2 arguments), Sqrt, Abs, ArcTan[x, y].
const Functions& elementary_functions();

// The table used when compile() gets none: the elementary functions plus everything
// added by register_functions (install_default_backends adds the Boost.Math special
// functions). Register at startup, before evaluating in other threads.
const Functions& default_functions();
void register_functions(const Functions& extra);

// Sample table of InterpolatingFunction[{a, b}, {{t, y}, ...}], sorted by t.
struct Samples {
    std::vector<double> t, y;
    // Cubic (4-point Lagrange) interpolation; NaN outside [t.front(), t.back()].
    double operator()(double at) const;
};
// Throws Unsupported unless `f` is a well-formed InterpolatingFunction with numeric data.
std::shared_ptr<const Samples> interpolating_samples(const ExprPtr& f);

class Compiled {
public:
    double eval(const double* slots) const;

    enum class Op { Const, Slot, Plus, Times, Power, Call1, Call2, Interpolate };
    Op op = Op::Const;
    double value = 0.0;
    std::size_t index = 0;
    Fn1 f1 = nullptr;
    Fn2 f2 = nullptr;
    std::shared_ptr<const Samples> samples;  // Interpolate
    std::vector<Compiled> args;
};

// Throws Unsupported for symbols outside `slots` (other than Pi and E) and unknown heads.
// InterpolatingFunction[...][u] compiles to interpolation of its samples.
Compiled compile(const ExprPtr& e, const Slots& slots,
                 const Functions& functions = default_functions());

// Value of an expression without free variables; throws Unsupported otherwise.
double constant(const ExprPtr& e, const Functions& functions = default_functions());

// 12 significant digits as an exact decimal rational (until Expr has a Real kind).
// Throws NonFinite for inf/NaN.
ExprPtr decimal(double value);

}  // namespace symats::numeric
