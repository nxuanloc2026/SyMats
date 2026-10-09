// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "symats/expr.h"
#include "symats/session.h"
#include "symats/text.h"

namespace {

void print_result(const symats::StatementResult& res) {
    if (!res.error.empty()) {
        std::cerr << "Error: " << res.error << "\n";
    }
    if (res.visible()) {
        std::cout << "Out[" << res.line << "] = " << symats::to_text(res.output) << "\n";
    }
}

int run_script(symats::Session& session, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filepath << "'\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    bool has_error = false;
    try {
        std::vector<symats::Statement> stmts = symats::parse_cell(content);
        for (const auto& stmt : stmts) {
            symats::StatementResult res = session.run(stmt.expr, stmt.suppressed);
            if (!res.error.empty()) {
                has_error = true;
            }
            print_result(res);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return has_error ? 1 : 0;
}

void run_repl(symats::Session& session) {
    std::cout << "Symats 0.1\nType 'quit' or 'exit' to exit.\n\n";
    std::string line;
    while (true) {
        std::cout << "In[" << session.next_line() << "]:= " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }
        if (line == "quit" || line == "exit") {
            break;
        }
        if (line.empty()) {
            continue;
        }
        try {
            std::vector<symats::Statement> stmts = symats::parse_cell(line);
            for (const auto& stmt : stmts) {
                symats::StatementResult res = session.run(stmt.expr, stmt.suppressed);
                print_result(res);
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    symats::Session session;
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: symats-cli [script.sym]\n"
                      << "Interactive symbolic mathematics REPL and script runner.\n";
            return 0;
        }
        return run_script(session, arg);
    }
    run_repl(session);
    return 0;
}
