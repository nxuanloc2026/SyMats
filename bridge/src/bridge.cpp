// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/bridge.h"

#include <sstream>
#include <vector>

#include "symats/latex.h"
#include "symats/mathjson.h"
#include "symats/text.h"

namespace symats {

namespace {

std::string escape_json(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u00" << (static_cast<int>(c) < 16 ? "0" : "")
                       << std::hex << static_cast<int>(c);
                } else {
                    ss << c;
                }
                break;
        }
    }
    return ss.str();
}

} // namespace

Bridge::Bridge() {}

std::string Bridge::eval_cell(const std::string& input_json_or_text) {
    std::vector<Statement> statements;
    try {
        statements = parse_cell(input_json_or_text);
    } catch (const std::exception& e) {
        std::ostringstream err_json;
        err_json << "[{"
                 << "\"line\":1,"
                 << "\"input\":\"" << escape_json(input_json_or_text) << "\","
                 << "\"output\":\"\","
                 << "\"latex\":\"\","
                 << "\"mathjson\":\"\","
                 << "\"status\":\"unverified\","
                 << "\"suppressed\":false,"
                 << "\"visible\":true,"
                 << "\"error\":\"" << escape_json(e.what()) << "\""
                 << "}]";
        return err_json.str();
    }

    if (statements.empty()) {
        return "[]";
    }

    std::ostringstream json;
    json << "[";
    bool first = true;
    for (const auto& stmt : statements) {
        auto res = session_.run(stmt.expr, stmt.suppressed);
        if (!first) json << ",";
        first = false;

        std::string input_text = stmt.expr ? to_text(stmt.expr) : "";
        std::string latex_str;
        std::string mathjson_str;
        std::string text_out;
        if (res.output) {
            latex_str = symats::to_latex(res.output);
            mathjson_str = symats::to_mathjson(res.output);
            text_out = symats::to_text(res.output);
        }

        std::string status_str = "unverified";
        if (res.status == ResultStatus::Exact || res.status == ResultStatus::Verified) {
            status_str = "verified";
        } else if (res.status == ResultStatus::Numeric) {
            status_str = "numeric";
        }

        json << "{"
             << "\"line\":" << res.line << ","
             << "\"input\":\"" << escape_json(input_text) << "\","
             << "\"output\":\"" << escape_json(text_out) << "\","
             << "\"latex\":\"" << escape_json(latex_str) << "\","
             << "\"mathjson\":\"" << escape_json(mathjson_str) << "\","
             << "\"status\":\"" << escape_json(status_str) << "\","
             << "\"suppressed\":" << (stmt.suppressed ? "true" : "false") << ","
             << "\"visible\":" << (res.visible() ? "true" : "false") << ","
             << "\"error\":\"" << escape_json(res.error) << "\""
             << "}";
    }
    json << "]";
    return json.str();
}

std::string Bridge::restart() {
    session_.restart();
    return "{\"status\":\"restarted\"}";
}

std::string Bridge::workspace() {
    auto symbols = session_.user_symbols();
    std::ostringstream json;
    json << "[";
    bool first = true;
    for (const auto& sym : symbols) {
        if (!first) json << ",";
        first = false;
        json << "\"" << escape_json(sym) << "\"";
    }
    json << "]";
    return json.str();
}

std::string Bridge::to_latex(const std::string& expr_text) {
    try {
        auto expr = parse_text(expr_text);
        return symats::to_latex(expr);
    } catch (const std::exception& e) {
        return "";
    }
}

std::string Bridge::to_mathjson(const std::string& expr_text) {
    try {
        auto expr = parse_text(expr_text);
        return symats::to_mathjson(expr);
    } catch (const std::exception& e) {
        return "";
    }
}

} // namespace symats
