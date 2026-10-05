// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/cell.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

#include "symats/text.h"

namespace symats {
namespace {

// Replaces % history shortcuts (% -> Out[], %% -> Out[-2], %n -> Out[n], %-n -> Out[-n])
std::string preprocess_history(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '%') {
            std::size_t count = 0;
            while (i < text.size() && text[i] == '%') {
                ++count;
                ++i;
            }
            if (count == 1) {
                // Check if followed by digits or minus and digits
                if (i < text.size() && (std::isdigit(static_cast<unsigned char>(text[i])) ||
                                        (text[i] == '-' && i + 1 < text.size() &&
                                         std::isdigit(static_cast<unsigned char>(text[i + 1]))))) {
                    std::size_t num_start = i;
                    if (text[i] == '-') ++i;
                    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
                    out += "Out[" + std::string(text.substr(num_start, i - num_start)) + "]";
                } else {
                    out += "Out[]";
                }
            } else if (count == 2) {
                out += "Out[-2]";
            } else {
                out += "Out[-" + std::to_string(count) + "]";
            }
        } else {
            out += text[i++];
        }
    }
    return out;
}

bool ends_with_operator(std::string_view s) {
    std::size_t i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
    if (i == 0) return false;
    char last = s[i - 1];
    if (last == '+' || last == '-' || last == '*' || last == '/' || last == '^' ||
        last == '=' || last == '<' || last == '>' || last == '.') return true;
    if (i >= 2) {
        std::string_view last2 = s.substr(i - 2, 2);
        if (last2 == "->" || last2 == ":=" || last2 == "==" || last2 == "!=" ||
            last2 == "<=" || last2 == ">=" || last2 == "&&" || last2 == "||" || last2 == "/.")
            return true;
    }
    return false;
}

}  // namespace

std::vector<ParsedStatement> parse_cell(std::string_view cell_text) {
    std::vector<ParsedStatement> statements;
    std::string processed = preprocess_history(cell_text);

    int depth_paren = 0;
    int depth_bracket = 0;
    int depth_brace = 0;

    std::string current_stmt;
    bool suppressed = false;

    std::size_t i = 0;
    while (i < processed.size()) {
        char c = processed[i];

        if (c == '(') ++depth_paren;
        else if (c == ')') { if (depth_paren > 0) --depth_paren; }
        else if (c == '[') ++depth_bracket;
        else if (c == ']') { if (depth_bracket > 0) --depth_bracket; }
        else if (c == '{') ++depth_brace;
        else if (c == '}') { if (depth_brace > 0) --depth_brace; }

        const bool inside_grouping = (depth_paren > 0 || depth_bracket > 0 || depth_brace > 0);

        if (c == ';' && !inside_grouping) {
            // Semicolon suppresses output for current statement
            suppressed = true;
            // Trim whitespace
            std::size_t start = current_stmt.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                std::size_t end = current_stmt.find_last_not_of(" \t\n\r");
                std::string stmt_text = current_stmt.substr(start, end - start + 1);
                ParsedStatement ps;
                ps.raw_text = stmt_text;
                ps.expr = parse_text(stmt_text);
                ps.visible = false;
                statements.push_back(std::move(ps));
            }
            current_stmt.clear();
            suppressed = false;
            ++i;
            continue;
        }

        if (c == '\n' && !inside_grouping && !ends_with_operator(current_stmt)) {
            // Newline separates statements unless inside grouping or after trailing operator
            std::size_t start = current_stmt.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                std::size_t end = current_stmt.find_last_not_of(" \t\n\r");
                std::string stmt_text = current_stmt.substr(start, end - start + 1);
                ParsedStatement ps;
                ps.raw_text = stmt_text;
                ps.expr = parse_text(stmt_text);
                ps.visible = !suppressed;
                statements.push_back(std::move(ps));
            }
            current_stmt.clear();
            suppressed = false;
            ++i;
            continue;
        }

        current_stmt += c;
        ++i;
    }

    // Process remaining statement
    std::size_t start = current_stmt.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        std::size_t end = current_stmt.find_last_not_of(" \t\n\r");
        std::string stmt_text = current_stmt.substr(start, end - start + 1);
        ParsedStatement ps;
        ps.raw_text = stmt_text;
        ps.expr = parse_text(stmt_text);
        ps.visible = !suppressed;
        statements.push_back(std::move(ps));
    }

    return statements;
}

}  // namespace symats
