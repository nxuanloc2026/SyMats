// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/bridge.h"

#include <cstdlib>
#include <cstring>

namespace {

char* duplicate_string(const std::string& str) {
    char* copy = static_cast<char*>(std::malloc(str.size() + 1));
    if (!copy) return nullptr;
    std::memcpy(copy, str.c_str(), str.size() + 1);
    return copy;
}

} // namespace

extern "C" {

symats_bridge_t symats_bridge_create(void) {
    return new (std::nothrow) symats::Bridge();
}

void symats_bridge_destroy(symats_bridge_t bridge) {
    delete static_cast<symats::Bridge*>(bridge);
}

char* symats_bridge_eval_cell(symats_bridge_t bridge, const char* input_json_or_text) {
    if (!bridge || !input_json_or_text) return nullptr;
    auto* b = static_cast<symats::Bridge*>(bridge);
    std::string res = b->eval_cell(input_json_or_text);
    return duplicate_string(res);
}

char* symats_bridge_restart(symats_bridge_t bridge) {
    if (!bridge) return nullptr;
    auto* b = static_cast<symats::Bridge*>(bridge);
    std::string res = b->restart();
    return duplicate_string(res);
}

char* symats_bridge_workspace(symats_bridge_t bridge) {
    if (!bridge) return nullptr;
    auto* b = static_cast<symats::Bridge*>(bridge);
    std::string res = b->workspace();
    return duplicate_string(res);
}

char* symats_bridge_to_latex(symats_bridge_t bridge, const char* expr_text) {
    if (!bridge || !expr_text) return nullptr;
    auto* b = static_cast<symats::Bridge*>(bridge);
    std::string res = b->to_latex(expr_text);
    return duplicate_string(res);
}

char* symats_bridge_to_mathjson(symats_bridge_t bridge, const char* expr_text) {
    if (!bridge || !expr_text) return nullptr;
    auto* b = static_cast<symats::Bridge*>(bridge);
    std::string res = b->to_mathjson(expr_text);
    return duplicate_string(res);
}

void symats_bridge_free_string(char* str) {
    std::free(str);
}

} // extern "C"
