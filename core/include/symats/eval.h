// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// The evaluator: definitions, rules, attributes, built-in functions, and dispatch
// of math heads (Integrate, Solve, DSolve, ...) to registered MathBackends.
//
// Evaluation of a Normal expression f(a1, ..., an):
//   1. evaluate the head; evaluate the arguments unless held (HoldFirst/HoldRest/HoldAll);
//   2. splice Sequence(...) arguments;
//   3. if f is Listable and some arguments are lists of equal length, thread over them;
//   4. try, in order: built-in function, user definitions (exact ones before
//      pattern ones), backends. The first that changes the expression wins;
//   5. repeat on the result until nothing changes (with depth/iteration limits).
#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "symats/backend.h"
#include "symats/expr.h"

namespace symats {

struct EvalResult;

namespace attr {
enum : unsigned {
    HoldFirst = 1u << 0,
    HoldRest = 1u << 1,
    HoldAll = HoldFirst | HoldRest,
    Listable = 1u << 2,
    Flat = 1u << 3,       // informational for now (canonical Plus/Times are already flat)
    Orderless = 1u << 4,  // informational for now
    Protected = 1u << 5,  // user definitions on this symbol are refused
    SequenceHold = 1u << 6,
};
}  // namespace attr

class EvaluationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Context {
public:
    // A built-in receives the expression with evaluated head and arguments and
    // returns the rewritten expression, or nullptr if it does not apply.
    using Builtin = std::function<ExprPtr(const ExprPtr&, Context&)>;

    Context();  // installs the standard attributes and built-ins

    // Own values: x = 5.
    void set_value(const std::string& symbol, ExprPtr value);
    ExprPtr value(const std::string& symbol) const;  // nullptr if none

    // Down values: f(x_) := x^2. A definition with a structurally equal lhs replaces
    // the old one. Definitions without patterns are tried before pattern ones.
    void add_definition(const std::string& symbol, ExprPtr lhs, ExprPtr rhs);
    void clear(const std::string& symbol);

    void set_attributes(const std::string& symbol, unsigned attributes);
    unsigned attributes(const std::string& symbol) const;

    void set_builtin(const std::string& head, Builtin fn);

    // Unprotected symbols that have an own value or definitions, sorted by name.
    std::vector<std::string> user_symbols() const;

    BackendRegistry& backends() { return backends_; }
    const BackendRegistry& backends() const { return backends_; }

    // Limits protecting against runaway recursion (f(x_) := f(x) + 1) and
    // non-terminating rewriting (f(x_) := f(x)).
    std::size_t max_depth = 400;
    std::size_t max_iterations = 100000;

    // Worst ResultStatus seen since the last reset (see evaluate_top).
    ResultStatus status() const { return status_; }
    void reset_status() { status_ = ResultStatus::Exact; }
    void note_status(ResultStatus s) { status_ = worst(status_, s); }

private:
    friend ExprPtr evaluate(const ExprPtr& e, Context& ctx);
    friend EvalResult evaluate_top(const ExprPtr& e, Context& ctx);
    friend struct EvalStep;

    struct Definition {
        ExprPtr lhs;
        ExprPtr rhs;
    };
    struct SymbolData {
        ExprPtr own_value;
        std::vector<Definition> exact;     // lhs without patterns
        std::vector<Definition> patterns;  // lhs with patterns, in definition order
        unsigned attributes = 0;
    };

    const SymbolData* find(const std::string& symbol) const;
    SymbolData& data(const std::string& symbol);

    std::unordered_map<std::string, SymbolData> symbols_;
    std::unordered_map<std::string, Builtin> builtins_;
    BackendRegistry backends_;
    ResultStatus status_ = ResultStatus::Exact;
    std::size_t depth_ = 0;
    std::size_t iterations_ = 0;
};

// Evaluate an expression in a context. Throws EvaluationError when a limit is hit
// or a definition is refused (e.g. on a Protected symbol).
ExprPtr evaluate(const ExprPtr& e, Context& ctx);

struct EvalResult {
    ExprPtr value;
    ResultStatus status;
};

// Evaluate a top-level input (one notebook cell): resets the status and the
// iteration budget, evaluates, and reports how trustworthy the result is.
EvalResult evaluate_top(const ExprPtr& e, Context& ctx);

}  // namespace symats
