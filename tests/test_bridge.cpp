// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include <cstdlib>
#include <string>

#include "symats/bridge.h"
#include "test.h"

using namespace symats;

TEST_CASE("Bridge: JSON cell evaluation and multi-statement persistence") {
    Bridge bridge;

    // Statement 1: a = 5; b = 10; a + b
    std::string req1 = R"({"action":"eval_cell","code":"a = 5;\nb = 10;\na + b"})";
    std::string res1 = bridge.handle_json_message(req1);

    CHECK(res1.find("\"results\":[") != std::string::npos);
    CHECK(res1.find("\"line\":1") != std::string::npos);
    CHECK(res1.find("\"suppressed\":true") != std::string::npos);
    CHECK(res1.find("\"line\":3") != std::string::npos);
    CHECK(res1.find("\"text\":\"15\"") != std::string::npos);
    CHECK(res1.find("\"latex\":\"15\"") != std::string::npos);

    // Workspace action
    std::string req_ws = R"({"action":"workspace"})";
    std::string res_ws = bridge.handle_json_message(req_ws);
    CHECK(res_ws.find("\"symbols\":[\"a\",\"b\"]") != std::string::npos ||
          res_ws.find("\"symbols\":[\"b\",\"a\"]") != std::string::npos);

    // Restart action
    std::string req_rs = R"({"action":"restart"})";
    std::string res_rs = bridge.handle_json_message(req_rs);
    CHECK_EQ(res_rs, std::string("{\"status\":\"ok\"}"));

    // Workspace after restart
    std::string res_ws2 = bridge.handle_json_message(req_ws);
    CHECK_EQ(res_ws2, std::string("{\"symbols\":[]}"));
}

TEST_CASE("Bridge: to_latex and to_mathjson helper actions") {
    Bridge bridge;

    std::string req_latex = R"({"action":"to_latex","text":"Sin[x] + 1/2"})";
    std::string res_latex = bridge.handle_json_message(req_latex);
    CHECK(res_latex.find("\\sin") != std::string::npos);

    std::string req_mjson = R"({"action":"to_mathjson","text":"Sin[x]"})";
    std::string res_mjson = bridge.handle_json_message(req_mjson);
    CHECK(res_mjson.find("[\"Sin\", \"x\"]") != std::string::npos ||
          res_mjson.find("[\"Sin\",\"x\"]") != std::string::npos);
}

TEST_CASE("Bridge: C FFI interface for Tauri and WebAssembly") {
    SymatsBridge* b = symats_bridge_create();
    CHECK(b != nullptr);

    const char* req = "{\"action\":\"eval_cell\",\"code\":\"x = 3; x^2\"}";
    char* res = symats_bridge_eval(b, req);
    CHECK(res != nullptr);
    std::string res_str(res);
    CHECK(res_str.find("\"text\":\"9\"") != std::string::npos);

    symats_bridge_free_string(res);
    symats_bridge_free(b);
}
