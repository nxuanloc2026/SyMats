// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <cctype>
#include <iostream>
#include <string>

#include "symats/session.h"
#include "symats/text.h"

namespace {

bool unclosed_brackets(std::string_view s) {
    int depth = 0;
    for (char c : s) {
        if (c == '[' || c == '(' || c == '{') depth++;
        else if (c == ']' || c == ')' || c == '}') {
            if (depth > 0) depth--;
        }
    }
    return depth > 0;
}

}  // namespace

int main() {
    using namespace symats;

    Session session;
    std::cout << "Symats 0.1.0 Interactive REPL\n";
    std::cout << "Type 'Quit' or 'Exit' or press Ctrl+D to exit.\n\n";

    std::string line;
    std::string cell_buffer;

    while (true) {
        if (cell_buffer.empty()) {
            std::cout << "In[" << session.next_line() << "]:= ";
        } else {
            std::cout << "  ...   ";
        }
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (cell_buffer.empty() && (line == "Quit" || line == "Exit" || line == "quit" || line == "exit")) {
            break;
        }

        cell_buffer += line;

        if (unclosed_brackets(cell_buffer)) {
            cell_buffer += "\n";
            continue;
        }

        try {
            std::vector<Statement> stmts = parse_cell(cell_buffer);
            std::vector<StatementResult> results = session.run_cell(stmts);

            for (const auto& res : results) {
                if (!res.ok()) {
                    std::cerr << "Error: " << res.error << "\n";
                } else if (res.visible()) {
                    std::cout << "Out[" << res.line << "]= " << to_text(res.output) << "\n";
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "Parse Error: " << ex.what() << "\n";
        }

        cell_buffer.clear();
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
