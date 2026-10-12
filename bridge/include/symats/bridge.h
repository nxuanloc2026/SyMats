// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include <string>

#include "symats/session.h"

namespace symats {

class Bridge {
public:
    Bridge();
    ~Bridge() = default;

    std::string eval_cell(const std::string& input_json_or_text);
    std::string restart();
    std::string workspace();
    std::string to_latex(const std::string& expr_text);
    std::string to_mathjson(const std::string& expr_text);

private:
    Session session_;
};

} // namespace symats

#ifdef __cplusplus
extern "C" {
#endif

typedef void* symats_bridge_t;

symats_bridge_t symats_bridge_create(void);
void symats_bridge_destroy(symats_bridge_t bridge);
char* symats_bridge_eval_cell(symats_bridge_t bridge, const char* input_json_or_text);
char* symats_bridge_restart(symats_bridge_t bridge);
char* symats_bridge_workspace(symats_bridge_t bridge);
char* symats_bridge_to_latex(symats_bridge_t bridge, const char* expr_text);
char* symats_bridge_to_mathjson(symats_bridge_t bridge, const char* expr_text);
void symats_bridge_free_string(char* str);

#ifdef __cplusplus
}
#endif
