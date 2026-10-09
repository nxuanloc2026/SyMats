// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/verification.h"

#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
ResultStatus status(const char* request, const char* result) {
    return verification_status(parse_text(request), parse_text(result));
}
}  // namespace

TEST_CASE("Verification: exact checks give Verified") {
    CHECK(status("Integrate[x^2, x]", "x^3/3") == ResultStatus::Verified);
    CHECK(status("Factor[x^2 - 1]", "(x - 1)*(x + 1)") == ResultStatus::Verified);
    CHECK(status("Solve[2*x == 4, x]", "{{x -> 2}}") == ResultStatus::Verified);
    CHECK(status("Inverse[{{1, 2}, {3, 4}}]", "{{-2, 1}, {3/2, -1/2}}") == ResultStatus::Verified);
    CHECK(verify_backend_result(parse_text("Integrate[x^2, x]"), parse_text("x^3/3")));
}

TEST_CASE("Verification: numeric fallback when the exact check cannot decide") {
    // Needs Sin^2 + Cos^2 == 1, which the exact simplifier does not know.
    CHECK(status("Integrate[Sin[x]^2, x]", "x/2 - Sin[2*x]/4") == ResultStatus::Numeric);
    CHECK(!verify_backend_result(parse_text("Integrate[Sin[x]^2, x]"), parse_text("x/2 - Sin[2*x]/4")));
    CHECK(status("Det[{{Sin[x], Cos[x]}, {-Cos[x], Sin[x]}}]", "1") == ResultStatus::Numeric);
    CHECK(status("Solve[Sin[x] == 1/2, x]", "{{x -> Pi/6}}") == ResultStatus::Numeric);
    CHECK(status("Integrate[Log[x], x]", "x*Log[x] - x") != ResultStatus::Unverified);
}

TEST_CASE("Verification: definite integrals are checked by quadrature") {
    CHECK(status("Integrate[x^2, {x, 0, 1}]", "1/3") == ResultStatus::Numeric);
    CHECK(status("Integrate[Exp[x], {x, 0, 1}]", "E - 1") == ResultStatus::Numeric);
    CHECK(status("Integrate[a*x, {x, 0, b}]", "a*b^2/2") == ResultStatus::Numeric);  // parameters
    CHECK(status("Integrate[x^2, {x, 0, 1}]", "1/2") == ResultStatus::Unverified);
}

TEST_CASE("Verification: wrong results stay Unverified") {
    CHECK(status("Integrate[x^2, x]", "x^3") == ResultStatus::Unverified);
    CHECK(status("Integrate[Sin[x]^2, x]", "x/2 + Sin[2*x]/4") == ResultStatus::Unverified);
    CHECK(status("Solve[x^2 == 2, x]", "{{x -> 14142/10000}}") == ResultStatus::Unverified);
    CHECK(status("Det[{{Sin[x], Cos[x]}, {-Cos[x], Sin[x]}}]", "2") == ResultStatus::Unverified);
    CHECK(status("Integrate[f[x], x]", "g[x]") == ResultStatus::Unverified);  // not numeric
    CHECK(status("Plot[x, {x, 0, 1}]", "x") == ResultStatus::Unverified);     // no check exists
}

TEST_CASE("Verification: DSolve solutions substituted into equations and conditions") {
    CHECK(status("DSolve[y'[x] == y[x], y[x], x]", "{{y[x] -> C1*Exp[x]}}") == ResultStatus::Verified);
    CHECK(status("DSolve[y''[x] + y[x] == 0, y[x], x]", "{{y[x] -> C1*Cos[x] + C2*Sin[x]}}") ==
          ResultStatus::Verified);
    CHECK(status("DSolve[{y'[x] == y[x], y[0] == 2}, y[x], x]", "{{y[x] -> 2*Exp[x]}}") == ResultStatus::Verified);
    CHECK(status("DSolve[{x'[t] == y[t], y'[t] == -x[t]}, {x[t], y[t]}, t]",
                 "{{x[t] -> C1*Cos[t] + C2*Sin[t], y[t] -> C2*Cos[t] - C1*Sin[t]}}") == ResultStatus::Verified);
    CHECK(status("DSolve[D[u[x, t], t] == k*D[u[x, t], {x, 2}], u[x, t], {x, t}]",
                 "{{u[x, t] -> Exp[-k*t]*Sin[x]}}") == ResultStatus::Verified);
    // Wrong initial value, wrong frequency, a branch that fails.
    CHECK(status("DSolve[{y'[x] == y[x], y[0] == 2}, y[x], x]", "{{y[x] -> 3*Exp[x]}}") == ResultStatus::Unverified);
    CHECK(status("DSolve[y''[x] == -y[x], y[x], x]", "{{y[x] -> Sin[2*x]}}") == ResultStatus::Unverified);
    CHECK(status("DSolve[y'[x] == y[x], y[x], x]", "{{y[x] -> Exp[x]}, {y[x] -> x}}") == ResultStatus::Unverified);
}
