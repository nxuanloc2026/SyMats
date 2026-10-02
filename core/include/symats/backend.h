// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// The MathBackend interface: the single door through which heavy mathematics
// (Giac, FLINT, SUNDIALS, ...) is reached. See AGENTS.md, "Rules for backends".
//
// core/ defines the interface; backend/ implements it. Only backend/ may include
// third-party math headers. Everything crosses this boundary as Symats Expr.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "symats/expr.h"

namespace symats {

// How much a result can be trusted. Ordered from best to worst; when several
// results combine, the overall status is the worst one.
enum class ResultStatus : unsigned char {
    Exact = 0,       // computed by Symats' own exact code (no backend involved)
    Verified = 1,    // computed by a backend and checked by Symats (T-013)
    Numeric = 2,     // numeric approximation
    Unverified = 3,  // computed by a backend, not (yet) checked
};

const char* to_string(ResultStatus s);
inline ResultStatus worst(ResultStatus a, ResultStatus b) { return a > b ? a : b; }

struct BackendResult {
    ExprPtr value;
    ResultStatus status = ResultStatus::Unverified;
    std::string backend;  // name of the backend that produced it
};

class MathBackend {
public:
    virtual ~MathBackend() = default;

    // Short identifier, e.g. "giac", "flint", "sundials".
    virtual std::string name() const = 0;

    // Whether this backend implements the given head, e.g. "Integrate".
    virtual bool supports(std::string_view head) const = 0;

    // Evaluate an expression whose head is supported. Arguments are already
    // evaluated. Return std::nullopt to decline (e.g. no closed form found).
    // Implementations should not throw; exceptions are treated as declining.
    virtual std::optional<BackendResult> evaluate(const ExprPtr& expr) = 0;
};

// Ordered list of backends. For each expression the first backend that supports
// its head and does not decline wins; this lets a function be moved to another
// backend (e.g. Factor to FLINT) by registering that backend earlier.
class BackendRegistry {
public:
    void add(std::shared_ptr<MathBackend> backend);
    bool empty() const { return backends_.empty(); }
    bool handles(std::string_view head) const;
    std::optional<BackendResult> try_evaluate(const ExprPtr& expr) const;
    const std::vector<std::shared_ptr<MathBackend>>& list() const { return backends_; }

private:
    std::vector<std::shared_ptr<MathBackend>> backends_;
};

}  // namespace symats
