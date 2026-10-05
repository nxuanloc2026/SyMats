// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// The kernel behind a notebook (AGENTS.md, "Notebook model").
//
// One Session per open notebook. Every cell's statements run in the same Context, so
// definitions persist across cells. Each statement gets a line number n (In[n]/Out[n]);
// `%` / Out[...] refer to earlier outputs. Errors are captured per statement and never
// kill the session.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "symats/eval.h"

namespace symats {

struct StatementResult {
    std::size_t line = 0;      // n in In[n] / Out[n]
    ExprPtr input;             // the statement as given
    ExprPtr output;            // evaluated result; nullptr if an error occurred
    ResultStatus status = ResultStatus::Exact;
    bool suppressed = false;   // statement ended with ';' — do not display
    std::string error;         // empty on success

    bool ok() const { return error.empty(); }
    // Whether the UI should show an output line (not suppressed, not Null, no error).
    bool visible() const;
};

struct Statement {
    ExprPtr expr;
    bool suppressed = false;
};

class Session {
public:
    Session();
    Session(const Session&) = delete;  // built-ins capture `this`
    Session& operator=(const Session&) = delete;

    // Run one statement. Never throws for evaluation errors (see StatementResult::error).
    StatementResult run(const ExprPtr& statement, bool suppressed = false);

    // Run all statements of a cell in order; an error in one does not stop the rest.
    std::vector<StatementResult> run_cell(const std::vector<Statement>& statements);

    // Next line number (1 for a fresh session).
    std::size_t next_line() const { return history_.size() + 1; }

    // Output of line n (1-based), or nullptr if out of range / that line failed.
    ExprPtr out(std::size_t n) const;
    const std::vector<StatementResult>& history() const { return history_; }

    // Restart kernel: forget every definition and the history. Backends stay registered.
    void restart();

    // Names of user-defined symbols (own values or definitions), sorted — for the
    // MATLAB-style workspace panel.
    std::vector<std::string> user_symbols() const { return context_->user_symbols(); }

    Context& context() { return *context_; }
    const Context& context() const { return *context_; }

private:
    void install_session_builtins();

    std::unique_ptr<Context> context_;
    std::vector<StatementResult> history_;
};

}  // namespace symats
