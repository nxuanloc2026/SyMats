// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "symats/expr.h"
#include "symats/session.h"
#include "symats/text.h"

namespace {

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

void print_result(const symats::StatementResult& res) {
    if (!res.error.empty()) {
        std::cerr << "Error: " << res.error << "\n";
    }
    if (res.visible()) {
        std::cout << "Out[" << res.line << "] = " << symats::to_text(res.output) << "\n";
    }
}

bool run_input(symats::Session& session, std::string_view input) {
    bool has_error = false;
    try {
        std::vector<symats::Statement> stmts = symats::parse_cell(input);
        for (const auto& stmt : stmts) {
            symats::StatementResult res = session.run(stmt.expr, stmt.suppressed);
            has_error = has_error || !res.error.empty();
            print_result(res);
        }
        return !has_error;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
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
    return run_input(session, buffer.str()) ? 0 : 1;
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
        const std::string_view command = trim(line);
        if (command == "quit" || command == "exit") {
            break;
        }
        if (command.empty()) {
            continue;
        }
        run_input(session, command);
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
