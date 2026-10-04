// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/cell.h"
#include "test.h"

using namespace symats;

TEST_CASE("Cell: lines and continuation become session statements") {
    auto cell = parse_cell("a = 2\nb = (a +\n 3)\n[a,\n b]\n");
    CHECK_EQ(cell.size(), std::size_t(3));
    CHECK_EQ(to_full_form(cell[0].expr), std::string("Set(a, 2)"));
    CHECK_EQ(to_full_form(cell[1].expr), std::string("Set(b, Plus(3, a))"));
    CHECK_EQ(to_full_form(cell[2].expr), std::string("List(a, b)"));
    CHECK(!cell[0].suppressed);
    CHECK(parse_cell(" \n\t\n").empty());
}

TEST_CASE("Cell: semicolons combine expressions and suppress trailing output") {
    auto cell = parse_cell("a = 1; b = a + 2\nb;\nc");
    CHECK_EQ(cell.size(), std::size_t(3));
    CHECK_EQ(to_full_form(cell[0].expr),
             std::string("CompoundExpression(Set(a, 1), Set(b, Plus(2, a)))"));
    CHECK(!cell[0].suppressed);
    CHECK_EQ(to_full_form(cell[1].expr),
             std::string("CompoundExpression(b, Null)"));
    CHECK(cell[1].suppressed);
    Session session;
    auto results = session.run_cell(cell);
    CHECK_EQ(to_full_form(results[0].output), std::string("3"));
    CHECK(!results[1].visible());
    CHECK_EQ(to_full_form(results[2].output), std::string("c"));
}

TEST_CASE("Cell: output history shorthand") {
    auto cell = parse_cell("%\n%%\n%12\n`%`\nOut(3)");
    CHECK_EQ(cell.size(), std::size_t(5));
    CHECK_EQ(to_full_form(cell[0].expr), std::string("Out()"));
    CHECK_EQ(to_full_form(cell[1].expr), std::string("Out(-2)"));
    CHECK_EQ(to_full_form(cell[2].expr), std::string("Out(12)"));
    CHECK_EQ(to_full_form(cell[3].expr), std::string("%"));
    CHECK_EQ(to_full_form(cell[4].expr), std::string("Out(3)"));
}

TEST_CASE("Cell: malformed separators and delimiters fail") {
    CHECK_THROWS(parse_cell("a;;b"));
    CHECK_THROWS(parse_cell(";a"));
    CHECK_THROWS(parse_cell("a)"));
    CHECK_THROWS(parse_cell("f(a"));
    CHECK_THROWS(parse_cell("`open"));
}
