// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/cell.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("Cell: parse_cell multi-statement splitting and semicolons") {
    std::string cell = "x = 5;\ny = 10\nx + y";
    auto stmts = parse_cell(cell);
    CHECK_EQ(stmts.size(), std::size_t(3));

    // Statement 1: x = 5; (visible = false)
    CHECK_EQ(stmts[0].raw_text, "x = 5");
    CHECK_EQ(to_full_form(stmts[0].expr), "Set(x, 5)");
    CHECK(!stmts[0].visible);

    // Statement 2: y = 10 (visible = true)
    CHECK_EQ(stmts[1].raw_text, "y = 10");
    CHECK_EQ(to_full_form(stmts[1].expr), "Set(y, 10)");
    CHECK(stmts[1].visible);

    // Statement 3: x + y (visible = true)
    CHECK_EQ(stmts[2].raw_text, "x + y");
    CHECK_EQ(to_full_form(stmts[2].expr), "Plus(x, y)");
    CHECK(stmts[2].visible);
}

TEST_CASE("Cell: parse_cell continuation inside brackets and trailing operators") {
    std::string cell1 = "Integrate[\n  x^2,\n  x\n]";
    auto stmts1 = parse_cell(cell1);
    CHECK_EQ(stmts1.size(), std::size_t(1));
    CHECK_EQ(to_full_form(stmts1[0].expr), "Integrate(Power(x, 2), x)");

    std::string cell2 = "x +\n  y";
    auto stmts2 = parse_cell(cell2);
    CHECK_EQ(stmts2.size(), std::size_t(1));
    CHECK_EQ(to_full_form(stmts2[0].expr), "Plus(x, y)");
}

TEST_CASE("Cell: parse_cell history shortcut replacement") {
    std::string cell = "a = %;\nb = %%\nc = %5;\nd = %-3";
    auto stmts = parse_cell(cell);
    CHECK_EQ(stmts.size(), std::size_t(4));

    CHECK_EQ(to_full_form(stmts[0].expr), "Set(a, Out())");
    CHECK_EQ(to_full_form(stmts[1].expr), "Set(b, Out(-2))");
    CHECK_EQ(to_full_form(stmts[2].expr), "Set(c, Out(5))");
    CHECK_EQ(to_full_form(stmts[3].expr), "Set(d, Out(-3))");
}
