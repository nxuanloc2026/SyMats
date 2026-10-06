// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include "symats/expr.h"

namespace symats {

// MathBackend defines the abstract interface for symbolic and mathematical backends
// (e.g. Giac, FLINT, SUNDIALS).
class MathBackend {
public:
    virtual ~MathBackend() = default;

    virtual ExprPtr integrate(const ExprPtr& expr) { return expr; }
    virtual ExprPtr solve(const ExprPtr& expr) { return expr; }
    virtual ExprPtr dsolve(const ExprPtr& expr) { return expr; }
    virtual ExprPtr limit(const ExprPtr& expr) { return expr; }
    virtual ExprPtr series(const ExprPtr& expr) { return expr; }
    virtual ExprPtr factor(const ExprPtr& expr) { return expr; }
    virtual ExprPtr simplify(const ExprPtr& expr) { return expr; }
};

using MathBackendPtr = std::shared_ptr<MathBackend>;

// Global backend registry functions in core/
void register_math_backend(MathBackendPtr backend);
MathBackendPtr get_math_backend();

}  // namespace symats
