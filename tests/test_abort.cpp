// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <atomic>
#include <chrono>
#include <memory>
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

namespace {
// Declines while an abort is requested, like a long-running numeric backend.
class SlowBackend final : public MathBackend {
public:
    std::string name() const override { return "slow"; }
    bool supports(std::string_view head) const override { return head == "Slow"; }
    std::optional<BackendResult> evaluate(const ExprPtr&) override {
        if (backend_abort_requested()) return std::nullopt;
        return BackendResult{make_integer(42), ResultStatus::Numeric, "slow"};
    }
};
}  // namespace

TEST_CASE("Abort: backends see the flag and the evaluator raises the abort") {
    Context ctx;
    ctx.backends().add(std::make_shared<SlowBackend>());
    CHECK(equal(evaluate(parse_text("Slow[]"), ctx), parse_text("42")));
    CHECK(!backend_abort_requested());  // only inside backend calls
    // Flag raised between the evaluator's own check and the backend call is the
    // realistic case; simulate it with a scope set around a direct evaluation.
    std::atomic<bool> flag{true};
    {
        const BackendAbortScope scope(&flag);
        CHECK(!SlowBackend().evaluate(parse_text("Slow[]")).has_value());
    }
    ctx.request_abort();
    CHECK_THROWS(evaluate(parse_text("Slow[]"), ctx));
}
