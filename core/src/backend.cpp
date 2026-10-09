// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/backend.h"

#include <exception>
#include <stdexcept>

namespace symats {

const char* to_string(ResultStatus s) {
    switch (s) {
        case ResultStatus::Exact: return "exact";
        case ResultStatus::Verified: return "verified";
        case ResultStatus::Numeric: return "numeric";
        case ResultStatus::Unverified: return "unverified";
    }
    return "unknown";
}

void BackendRegistry::add(std::shared_ptr<MathBackend> backend) {
    if (!backend) throw std::invalid_argument("BackendRegistry::add: null backend");
    backends_.push_back(std::move(backend));
}

bool BackendRegistry::handles(std::string_view head) const {
    for (const auto& b : backends_)
        if (b->supports(head)) return true;
    return false;
}

std::optional<BackendResult> BackendRegistry::try_evaluate(const ExprPtr& expr) const {
    if (!expr->is_normal() || !expr->head()->is_symbol()) return std::nullopt;
    const std::string& head = expr->head()->name();
    for (const auto& b : backends_) {
        if (!b->supports(head)) continue;
        std::optional<BackendResult> r;
        try {
            r = b->evaluate(expr);
        } catch (const std::exception&) {
            r.reset();  // a failing backend declines; the next one may succeed
        }
        if (r && r->value) {
            if (r->backend.empty()) r->backend = b->name();
            return r;
        }
    }
    return std::nullopt;
}

namespace {
thread_local const std::atomic<bool>* current_abort = nullptr;
}  // namespace

bool backend_abort_requested() {
    return current_abort && current_abort->load(std::memory_order_relaxed);
}

BackendAbortScope::BackendAbortScope(const std::atomic<bool>* flag) : previous_(current_abort) {
    current_abort = flag;
}

BackendAbortScope::~BackendAbortScope() { current_abort = previous_; }

}  // namespace symats
