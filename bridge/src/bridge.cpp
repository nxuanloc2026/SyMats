// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "symats/bridge.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "symats/latex.h"
#include "symats/mathjson.h"
#include "symats/text.h"

namespace symats {
namespace {

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string extract_field(std::string_view json, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string_view::npos) return {};
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
    if (pos >= json.size()) return {};
    if (json[pos] == '"') {
        pos++;
        std::size_t end = pos;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;
            } else if (json[end] == '"') {
                break;
            } else {
                end++;
            }
        }
        std::string raw(json.substr(pos, end - pos));
        std::string unescaped;
        unescaped.reserve(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                char next = raw[++i];
                if (next == 'n') unescaped += '\n';
                else if (next == 't') unescaped += '\t';
                else if (next == 'r') unescaped += '\r';
                else if (next == '"') unescaped += '"';
                else if (next == '\\') unescaped += '\\';
                else unescaped += next;
            } else {
                unescaped += raw[i];
            }
        }
        return unescaped;
    }
    std::size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']' && !std::isspace(static_cast<unsigned char>(json[end]))) {
        end++;
    }
    return std::string(json.substr(pos, end - pos));
}

std::string status_to_string(ResultStatus st) {
    switch (st) {
        case ResultStatus::Exact: return "exact";
        case ResultStatus::Verified: return "verified";
        case ResultStatus::Numeric: return "numeric";
        case ResultStatus::Unverified: return "unverified";
    }
    return "unverified";
}

}  // namespace

Bridge::Bridge() : session_(std::make_unique<Session>()) {}

Bridge::Bridge(std::unique_ptr<Session> session) : session_(std::move(session)) {
    if (!session_) session_ = std::make_unique<Session>();
}

Bridge::~Bridge() = default;

std::string Bridge::handle_json_message(std::string_view request_json) {
    std::string action = extract_field(request_json, "action");
    if (action.empty()) {
        if (!extract_field(request_json, "code").empty()) {
            action = "eval_cell";
        } else {
            action = "eval_cell";
        }
    }

    if (action == "restart") {
        session_->restart();
        return "{\"status\":\"ok\"}";
    }

    if (action == "workspace") {
        auto syms = session_->user_symbols();
        std::string out = "{\"symbols\":[";
        for (std::size_t i = 0; i < syms.size(); ++i) {
            if (i) out += ",";
            out += "\"" + escape_json(syms[i]) + "\"";
        }
        out += "]}";
        return out;
    }

    if (action == "to_latex") {
        std::string text = extract_field(request_json, "text");
        try {
            ExprPtr expr = parse_text(text);
            return "{\"latex\":\"" + escape_json(to_latex(expr)) + "\"}";
        } catch (const std::exception& e) {
            return "{\"error\":\"" + escape_json(e.what()) + "\"}";
        }
    }

    if (action == "to_mathjson") {
        std::string text = extract_field(request_json, "text");
        try {
            ExprPtr expr = parse_text(text);
            return "{\"mathjson\":" + to_mathjson(expr) + "}";
        } catch (const std::exception& e) {
            return "{\"error\":\"" + escape_json(e.what()) + "\"}";
        }
    }

    // Default action: eval_cell
    std::string code = extract_field(request_json, "code");
    if (code.empty() && !request_json.empty() && request_json.front() != '{') {
        code = std::string(request_json);
    }

    std::string out = "{\"results\":[";
    try {
        std::vector<Statement> stmts = parse_cell(code);
        for (std::size_t i = 0; i < stmts.size(); ++i) {
            if (i) out += ",";
            StatementResult res = session_->run(stmts[i].expr, stmts[i].suppressed);
            out += "{";
            out += "\"line\":" + std::to_string(res.line) + ",";
            out += "\"status\":\"" + status_to_string(res.status) + "\",";
            out += "\"suppressed\":" + std::string(res.suppressed ? "true" : "false") + ",";
            if (!res.error.empty()) {
                out += "\"error\":\"" + escape_json(res.error) + "\"";
            } else {
                out += "\"error\":\"\",";
                out += "\"text\":\"" + escape_json(res.output ? to_text(res.output) : "") + "\",";
                out += "\"latex\":\"" + escape_json(res.output ? to_latex(res.output) : "") + "\",";
                out += "\"mathjson\":" + (res.output ? to_mathjson(res.output) : "null");
            }
            out += "}";
        }
    } catch (const std::exception& e) {
        out = "{\"results\":[],\"error\":\"" + escape_json(e.what()) + "\"}";
        return out;
    }
    out += "]}";
    return out;
}

}  // namespace symats

extern "C" {

struct SymatsBridgeOpaque {
    std::unique_ptr<symats::Bridge> bridge;
};

SymatsBridge* symats_bridge_create(void) {
    auto b = new SymatsBridgeOpaque();
    b->bridge = std::make_unique<symats::Bridge>();
    return b;
}

void symats_bridge_free(SymatsBridge* bridge) {
    delete bridge;
}

char* symats_bridge_eval(SymatsBridge* bridge, const char* request_json) {
    if (!bridge || !bridge->bridge || !request_json) return nullptr;
    std::string res = bridge->bridge->handle_json_message(request_json);
    char* out = static_cast<char*>(malloc(res.size() + 1));
    if (out) {
        memcpy(out, res.c_str(), res.size() + 1);
    }
    return out;
}

void symats_bridge_free_string(char* str) {
    free(str);
}

}
