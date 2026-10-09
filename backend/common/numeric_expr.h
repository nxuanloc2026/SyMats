// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Fast double-precision evaluation of Expr trees for numeric backends (NDSolve,
// NIntegrate). An expression is compiled once against named slots (the free
// variables) and then evaluated many times. Elementary functions use <cmath>;
// special functions use Boost.Math (see docs/EXPR_SPEC.md §3.3).
#pragma once

#include <cstddef>
#include <functional>
#include <map>
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

class Compiled {
public:
    double eval(const double* slots) const;

    enum class Op { Const, Slot, Plus, Times, Power, Call1, Call2 };
    Op op = Op::Const;
    double value = 0.0;
    std::size_t index = 0;
    double (*f1)(double) = nullptr;
    double (*f2)(double, double) = nullptr;
    std::vector<Compiled> args;
};

// Throws Unsupported for symbols outside `slots` (other than Pi and E) and unknown heads.
Compiled compile(const ExprPtr& e, const Slots& slots);

// Value of an expression without free variables; throws Unsupported otherwise.
double constant(const ExprPtr& e);

// 12 significant digits as an exact decimal rational (until Expr has a Real kind).
// Throws NonFinite for inf/NaN.
ExprPtr decimal(double value);

}  // namespace symats::numeric
