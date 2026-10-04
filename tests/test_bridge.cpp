// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/bridge.h"
#include "test.h"

#include <string>

TEST_CASE("Bridge: session results, history, and restart") {
    symats_restart();
    CHECK(std::string(symats_run_text("x = 2", 0)).find("\"text\":\"2\"") != std::string::npos);
    CHECK(std::string(symats_run_text("x + 3", 0)).find("\"text\":\"5\"") != std::string::npos);
    CHECK(std::string(symats_run_text("7", 1)).find("\"visible\":false") != std::string::npos);
    CHECK(std::string(symats_user_symbols()).find("\"x\"") != std::string::npos);
    symats_restart();
    CHECK_EQ(std::string(symats_user_symbols()), std::string("[]"));
    CHECK(std::string(symats_run_text("x", 0)).find("\"text\":\"x\"") != std::string::npos);
}

TEST_CASE("Bridge: MathJSON and errors use JSON messages") {
    symats_restart();
    CHECK_EQ(std::string(symats_text_to_mathjson("1/2")),
             std::string("[\"Rational\",{\"num\":\"1\"},{\"num\":\"2\"}]"));
    CHECK_EQ(std::string(symats_mathjson_to_text("[\"Rational\",1,2]")), std::string("\"1/2\""));
    CHECK(std::string(symats_run_mathjson("[\"Add\",2,3]", 0)).find("\"text\":\"5\"")
          != std::string::npos);
    CHECK(std::string(symats_run_text("sin(", 0)).find("\"error\":") != std::string::npos);
}
