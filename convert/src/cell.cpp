// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/cell.h"

#include <cctype>
#include <stdexcept>
#include <string>

#include "symats/text.h"

namespace symats {
namespace {

bool continuation(char c) {
    switch (c) {
    case '+': case '-': case '*': case '/': case '^': case '=': case '<':
    case '>': case '&': case '|': case '.': case ',': case ':':
        return true;
    default: return false;
    }
}

std::string history_references(std::string_view source) {
    std::string result;
    bool quoted = false;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];
        if (c == '`') {
            result += c;
            if (quoted && i + 1 < source.size() && source[i + 1] == '`')
                result += source[++i];
            else quoted = !quoted;
        } else if (c == '%' && !quoted) {
            const std::size_t start = i;
            while (i + 1 < source.size() && source[i + 1] == '%') ++i;
            const std::size_t count = i - start + 1;
            if (count > 1) {
                result += "Out(-" + std::to_string(count) + ")";
            } else if (i + 1 < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[i + 1]))) {
                const std::size_t digits = i + 1;
                while (i + 1 < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[i + 1]))) ++i;
                result += "Out(" + std::string(source.substr(digits, i - digits + 1)) + ")";
            } else result += "Out()";
        } else result += c;
    }
    return result;
}

}  // namespace

std::vector<Statement> parse_cell(std::string_view text) {
    std::vector<Statement> statements;
    ExprList parts;
    std::string pending;
    std::string delimiters;
    bool quoted = false;
    bool trailing_semicolon = false;

    const auto trim_pending = [&]() {
        while (!pending.empty() && std::isspace(static_cast<unsigned char>(pending.back())))
            pending.pop_back();
        const auto first = pending.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) pending.clear();
        else if (first) pending.erase(0, first);
    };
    const auto add_part = [&]() {
        trim_pending();
        if (pending.empty()) return false;
        parts.push_back(parse_text(history_references(pending)));
        pending.clear();
        return true;
    };
    const auto finish = [&]() {
        add_part();
        if (parts.empty()) {
            if (trailing_semicolon) throw std::invalid_argument("empty statement before semicolon");
            return;
        }
        if (trailing_semicolon) parts.push_back(make_symbol("Null"));
        ExprPtr expression = parts.size() == 1 ? parts.front()
            : make_normal("CompoundExpression", std::move(parts));
        statements.push_back({expression, trailing_semicolon});
        parts.clear();
        trailing_semicolon = false;
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '`') {
            pending += c;
            if (quoted && i + 1 < text.size() && text[i + 1] == '`')
                pending += text[++i];
            else quoted = !quoted;
            continue;
        }
        if (!quoted) {
            if (c == '(' || c == '[' || c == '{') delimiters += c;
            else if (c == ')' || c == ']' || c == '}') {
                if (delimiters.empty() ||
                    (c == ')' && delimiters.back() != '(') ||
                    (c == ']' && delimiters.back() != '[') ||
                    (c == '}' && delimiters.back() != '{'))
                    throw std::invalid_argument("unmatched cell delimiter");
                delimiters.pop_back();
            }
            if (delimiters.empty() && c == ';') {
                if (!add_part()) throw std::invalid_argument("empty statement before semicolon");
                trailing_semicolon = true;
                continue;
            }
            if (delimiters.empty() && (c == '\n' || c == '\r')) {
                trim_pending();
                if (!pending.empty() && continuation(pending.back())) {
                    pending += ' ';
                    continue;
                }
                finish();
                continue;
            }
        }
        pending += c;
        if (!std::isspace(static_cast<unsigned char>(c))) trailing_semicolon = false;
    }
    if (quoted || !delimiters.empty()) throw std::invalid_argument("unterminated cell expression");
    finish();
    return statements;
}

}  // namespace symats
