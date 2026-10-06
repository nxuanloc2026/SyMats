// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// symats-cli: command-line REPL and script runner.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "symats/session.h"
#include "symats/text.h"

namespace {

void run_script(symats::Session& session, std::istream& in, bool interactive) {
    std::string line;
    while (true) {
        if (interactive) {
            std::cout << "In[" << session.next_line() << "]:= " << std::flush;
        }
        if (!std::getline(in, line)) break;
        if (line == "exit" || line == "quit") break;

        std::vector<symats::Statement> statements;
        try {
            statements = symats::parse_cell(line);
        } catch (const std::exception& ex) {
            std::cout << "Syntax error: " << ex.what() << "\n";
            continue;
        }

        auto results = session.run_cell(statements);
        for (const auto& r : results) {
            if (!r.ok()) {
                std::cout << "Error: " << r.error << "\n";
            } else if (r.visible()) {
                std::cout << "Out[" << r.line << "] = " << symats::to_text(r.output) << "\n";
            }
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    symats::Session session;

    if (argc > 1) {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Error: could not open file '" << argv[1] << "'\n";
            return 1;
        }
        run_script(session, file, false);
        return 0;
    }

    std::cout << "Symats 0.1 REPL\nType 'exit' or 'quit' or press Ctrl+D to exit.\n\n";
    run_script(session, std::cin, true);
    return 0;
}
