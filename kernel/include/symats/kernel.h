// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "symats/cell.h"
#include "symats/session.h"
#include "symats/text.h"

namespace symats {

struct KernelExecutionResult {
    bool ok = true;
    std::string text_output;
    std::string latex_output;
    std::string error_message;
    std::size_t line_number = 0;
};

class SymatsKernelEngine {
public:
    SymatsKernelEngine() = default;

    // Executes code cell in the session and returns execution results per statement
    std::vector<KernelExecutionResult> execute_cell(std::string_view cell_code) {
        std::vector<KernelExecutionResult> results;
        auto parsed_statements = parse_cell(cell_code);

        for (const auto& ps : parsed_statements) {
            KernelExecutionResult res;
            if (!ps.expr) {
                res.ok = false;
                res.error_message = "Syntax error in expression";
                results.push_back(std::move(res));
                continue;
            }

            auto stmt_res = session_.run(ps.expr);
            res.ok = stmt_res.ok();
            res.line_number = stmt_res.line;

            if (stmt_res.ok()) {
                if (ps.visible && stmt_res.visible() && stmt_res.output) {
                    res.text_output = to_text(stmt_res.output);
                    res.latex_output = "$$" + res.text_output + "$$";
                }
            } else {
                res.error_message = stmt_res.error;
            }

            results.push_back(std::move(res));
        }

        return results;
    }

    Session& session() { return session_; }
    const Session& session() const { return session_; }

private:
    Session session_;
};

}  // namespace symats
