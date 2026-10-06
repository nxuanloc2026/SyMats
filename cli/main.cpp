// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// symats-cli: command-line REPL and script runner for Symats.
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "symats/expr.h"
#include "symats/session.h"
#include "symats/text.h"

namespace {

std::string trim(std::string_view s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return std::string(s.substr(start, end - start));
}

void execute_line(symats::Session& session, std::string line) {
    bool suppressed = false;
    if (!line.empty() && line.back() == ';') {
        suppressed = true;
        line.pop_back();
        line = trim(line);
    }
    if (line.empty()) return;

    try {
        symats::ExprPtr input_expr = symats::parse_text(line);
        symats::StatementResult res = session.run(input_expr, suppressed);
        if (!res.ok()) {
            std::cerr << "Error: " << res.error << "\n";
        } else if (res.visible()) {
            std::cout << "Out[" << res.line << "]= " << symats::to_text(res.output) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void run_repl(symats::Session& session) {
    std::cout << "Symats 0.1 - Interactive Symbolic Math REPL\n";
    std::cout << "Type 'exit' or 'quit' to exit.\n\n";

    std::string line;
    while (true) {
        std::cout << "In[" << session.next_line() << "]:= " << std::flush;
        if (!std::getline(std::cin, line)) break;

        std::string trimmed = trim(line);
        if (trimmed == "exit" || trimmed == "quit") break;
        if (trimmed.empty()) continue;

        execute_line(session, trimmed);
    }
}

int run_file(symats::Session& session, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << filepath << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.rfind("(*", 0) == 0 || trimmed.rfind('#', 0) == 0)
            continue;

        std::cout << "In[" << session.next_line() << "]:= " << trimmed << "\n";
        execute_line(session, trimmed);
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    symats::Session session;

    if (argc > 1) {
        int status = 0;
        for (int i = 1; i < argc; ++i) {
            int ret = run_file(session, argv[i]);
            if (ret != 0) status = ret;
        }
        return status;
    }

    run_repl(session);
    return 0;
}
