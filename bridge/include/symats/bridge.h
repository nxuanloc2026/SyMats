// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

// UTF-8 C ABI for the WebAssembly worker. Each returned pointer remains valid
// until the next bridge call on the same worker. Results are JSON strings.
extern "C" {
const char* symats_run_text(const char* text, int suppressed);
const char* symats_run_mathjson(const char* math_json, int suppressed);
const char* symats_text_to_mathjson(const char* text);
const char* symats_mathjson_to_text(const char* math_json);
const char* symats_user_symbols();
void symats_restart();
}
