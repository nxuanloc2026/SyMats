// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/session.h"

#include <exception>

namespace symats {

bool StatementResult::visible() const {
    return ok() && !suppressed && output && !output->is_symbol("Null");
}

Session::Session() : context_(std::make_unique<Context>()) { install_session_builtins(); }

void Session::install_session_builtins() {
    // Out[] / Out[-k]: k-th previous line; Out[n]: line n. The statement being evaluated
    // is not in the history yet, so Out[-1] is the line just before it.
    context_->set_builtin("Out", [this](const ExprPtr& e, Context&) -> ExprPtr {
        long long n = -1;
        if (e->size() == 1) {
            if (!e->arg(0)->is_integer()) return nullptr;
            auto v = e->arg(0)->integer().to_int64();
            if (!v || *v == 0) throw EvaluationError("Out: line number must be non-zero");
            n = *v;
        } else if (e->size() != 0) {
            return nullptr;
        }
        const long long size = static_cast<long long>(history_.size());
        const long long index = n > 0 ? n : size + 1 + n;  // 1-based line number
        if (index < 1 || index > size)
            throw EvaluationError("Out[" + std::to_string(n) + "] does not exist");
        ExprPtr v = history_[static_cast<std::size_t>(index - 1)].output;
        if (!v) throw EvaluationError("Out[" + std::to_string(index) + "] failed");
        return v;
    });
    context_->set_attributes("Out", attr::Protected);
}

StatementResult Session::run(const ExprPtr& statement, bool suppressed) {
    StatementResult r;
    r.line = next_line();
    r.input = statement;
    r.suppressed = suppressed;
    try {
        EvalResult v = evaluate_top(statement, *context_);
        r.output = v.value;
        r.status = v.status;
    } catch (const std::exception& ex) {
        r.output = nullptr;
        r.error = ex.what();
        r.status = ResultStatus::Exact;
    }
    history_.push_back(r);
    return r;
}

std::vector<StatementResult> Session::run_cell(const std::vector<Statement>& statements) {
    std::vector<StatementResult> results;
    results.reserve(statements.size());
    for (const auto& s : statements) results.push_back(run(s.expr, s.suppressed));
    return results;
}

ExprPtr Session::out(std::size_t n) const {
    if (n == 0 || n > history_.size()) return nullptr;
    return history_[n - 1].output;
}

void Session::restart() {
    auto fresh = std::make_unique<Context>();
    for (const auto& b : context_->backends().list()) fresh->backends().add(b);
    fresh->max_depth = context_->max_depth;
    fresh->max_iterations = context_->max_iterations;
    context_ = std::move(fresh);
    history_.clear();
    install_session_builtins();
}

}  // namespace symats
