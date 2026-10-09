// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "default_backends.h"

#include <algorithm>
#include <cmath>

#include "symats/session.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
bool has(const std::vector<std::string>& names, const char* name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}
}  // namespace

TEST_CASE("Default backends: registered in build order and survive restart") {
    const auto names = default_backend_names();
    Session session;
    install_default_backends(session.context().backends());
    CHECK(session.context().backends().list().size() == names.size());
    session.restart();
    CHECK(session.context().backends().list().size() == names.size());
#ifdef SYMATS_TEST_BOOST
    CHECK(has(names, "odeint"));
    CHECK(has(names, "quadrature"));
    const auto r = session.run(parse_text("NIntegrate[Exp[-x^2], {x, -Infinity, Infinity}]"));
    CHECK(r.output && r.output->is_number());
    if (r.output && r.output->is_number())
        CHECK(std::abs(r.output->number().to_double() - std::sqrt(std::acos(-1.0))) < 1e-10);
    CHECK(r.status == ResultStatus::Numeric);
    const auto s = session.run(parse_text("NDSolve[{x'[t] == y[t], y'[t] == -x[t], x[0] == 1, y[0] == 0}, {x, y}, {t, 0, 1}]"));
    CHECK(s.output && s.output->has_head("List") && s.output->size() == 2);
#endif
#ifdef SYMATS_TEST_GIAC
    CHECK(has(names, "giac"));
#endif
    CHECK(!has(names, "nonexistent"));
}
