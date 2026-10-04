// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/bridge.h"

#include <exception>
#include <stdexcept>
#include <string>

#include "symats/mathjson.h"
#include "symats/session.h"
#include "symats/text.h"

namespace {
symats::Session session;
thread_local std::string response;

std::string quoted(std::string_view text) {
    std::string out = "\"";
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            out += "\\u00";
            out += digits[c >> 4];
            out += digits[c & 15];
        } else out += static_cast<char>(c);
    }
    out += '"';
    return out;
}

const char* result(const symats::StatementResult& value) {
    response = "{\"line\":" + std::to_string(value.line) +
               ",\"suppressed\":" + (value.suppressed ? "true" : "false") +
               ",\"visible\":" + (value.visible() ? "true" : "false") +
               ",\"status\":" + quoted(symats::to_string(value.status)) +
               ",\"error\":" + quoted(value.error);
    if (value.output) {
        response += ",\"mathJson\":" + symats::to_mathjson(value.output);
        response += ",\"text\":" + quoted(symats::to_text(value.output));
    }
    response += '}';
    return response.c_str();
}

template <class Parse>
const char* run(const char* source, int suppressed, Parse parse) {
    try {
        if (!source) throw std::invalid_argument("missing expression");
        return result(session.run(parse(source), suppressed != 0));
    } catch (const std::exception& error) {
        response = "{\"error\":" + quoted(error.what()) + "}";
        return response.c_str();
    }
}
}  // namespace

extern "C" const char* symats_run_text(const char* text, int suppressed) {
    return run(text, suppressed, symats::parse_text);
}
extern "C" const char* symats_run_mathjson(const char* json, int suppressed) {
    return run(json, suppressed, symats::parse_mathjson);
}
extern "C" const char* symats_text_to_mathjson(const char* text) {
    try {
        if (!text) throw std::invalid_argument("missing expression");
        response = symats::to_mathjson(symats::parse_text(text));
    } catch (const std::exception& error) {
        response = "{\"error\":" + quoted(error.what()) + "}";
    }
    return response.c_str();
}
extern "C" const char* symats_mathjson_to_text(const char* json) {
    try {
        if (!json) throw std::invalid_argument("missing expression");
        response = quoted(symats::to_text(symats::parse_mathjson(json)));
    } catch (const std::exception& error) {
        response = "{\"error\":" + quoted(error.what()) + "}";
    }
    return response.c_str();
}
extern "C" const char* symats_user_symbols() {
    response = "[";
    for (const auto& name : session.user_symbols()) {
        if (response.size() > 1) response += ',';
        response += quoted(name);
    }
    response += ']';
    return response.c_str();
}
extern "C" void symats_restart() { session.restart(); }
