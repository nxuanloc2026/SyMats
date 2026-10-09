// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "symats/session.h"

namespace symats {

class Bridge {
public:
    Bridge();
    explicit Bridge(std::unique_ptr<Session> session);
    ~Bridge();

    // Process a JSON request message string and return a JSON response string.
    // Actions:
    // - "eval_cell" / "eval": { "action": "eval_cell", "code": "cell_text" }
    //   returns: { "results": [ { "line": 1, "text": "...", "mathjson": "...", "latex": "...", "status": "exact", "suppressed": false, "error": "" } ] }
    // - "restart": { "action": "restart" } -> { "status": "ok" }
    // - "workspace": { "action": "workspace" } -> { "symbols": [ "a", "b" ] }
    // - "to_latex": { "action": "to_latex", "text": "Sin[x]" } -> { "latex": "\\sin\\left(x\\right)" }
    // - "to_mathjson": { "action": "to_mathjson", "text": "Sin[x]" } -> { "mathjson": "[\"Sin\", \"x\"]" }
    std::string handle_json_message(std::string_view request_json);

    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

private:
    std::unique_ptr<Session> session_;
};

}  // namespace symats

// C FFI interface for Tauri (Rust FFI) or WebAssembly (Emscripten)
extern "C" {

struct SymatsBridgeOpaque;
typedef struct SymatsBridgeOpaque SymatsBridge;

SymatsBridge* symats_bridge_create(void);
void symats_bridge_free(SymatsBridge* bridge);

// Process a JSON request string and return a dynamically allocated JSON response string.
// Caller must free the returned string with symats_bridge_free_string.
char* symats_bridge_eval(SymatsBridge* bridge, const char* request_json);
void symats_bridge_free_string(char* str);

}
