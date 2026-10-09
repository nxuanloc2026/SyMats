// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <chrono>
#include <thread>

#include "symats/eval.h"
#include "symats/session.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

TEST_CASE("Abort: a flagged context stops at the next step") {
    Context ctx;
    ctx.request_abort();
    CHECK_THROWS(evaluate(parse_text("1 + 2"), ctx));
    ctx.clear_abort();
    CHECK(equal(evaluate(parse_text("1 + 2"), ctx), parse_text("3")));
}

TEST_CASE("Abort: Session stops a long cell from another thread and keeps running") {
    Session session;
    session.context().max_iterations = 100000000;  // let the loop run until aborted
    session.context().max_depth = 100000;
    std::vector<Statement> cell = {
        {parse_text("a = 1"), false},
        {parse_text("f[n_] := f[n + 1]"), false},
        {parse_text("f[0]"), false},  // runs until the iteration limit, or the abort
        {parse_text("b = 2"), false},
    };
    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        session.abort();
    });
    const auto results = session.run_cell(cell);
    stopper.join();
    CHECK(results.size() == 3);  // b = 2 did not run
    CHECK(results.back().aborted);
    CHECK(!results.back().ok());
    CHECK(equal(session.run(parse_text("a")).output, parse_text("1")));  // definitions kept
    CHECK(session.run(parse_text("b")).output->is_symbol("b"));
}
