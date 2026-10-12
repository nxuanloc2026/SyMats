// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/bridge.h"
#include "test.h"

#include <cstring>
#include <string>

TEST_CASE("Bridge C++ and C API cell evaluation and workspace") {
    symats::Bridge bridge;

    std::string res1 = bridge.eval_cell("a = 5\nb = a + 3;");
    CHECK(res1.find("\"line\":1") != std::string::npos);
    CHECK(res1.find("\"input\":\"a = 5\"") != std::string::npos);
    CHECK(res1.find("\"output\":\"5\"") != std::string::npos);

    std::string ws = bridge.workspace();
    CHECK(ws.find("\"a\"") != std::string::npos);

    std::string latex = bridge.to_latex("Sin[x] + x^2");
    CHECK(latex.find("\\sin") != std::string::npos);

    // Test C API
    symats_bridge_t c_bridge = symats_bridge_create();
    CHECK(c_bridge != nullptr);

    char* eval_res = symats_bridge_eval_cell(c_bridge, "x = 10\nx + 2");
    CHECK(eval_res != nullptr);
    CHECK(std::string(eval_res).find("12") != std::string::npos);
    symats_bridge_free_string(eval_res);

    char* c_ws = symats_bridge_workspace(c_bridge);
    CHECK(c_ws != nullptr);
    CHECK(std::string(c_ws).find("\"x\"") != std::string::npos);
    symats_bridge_free_string(c_ws);

    symats_bridge_destroy(c_bridge);
}
