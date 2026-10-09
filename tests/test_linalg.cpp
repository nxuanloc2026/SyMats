// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/eval.h"
#include "symats/text.h"
#include "test.h"

using namespace symats;

namespace {
bool same(const char* input, const char* expected) {
    Context ctx;
    return equal(evaluate(parse_text(input), ctx), evaluate(parse_text(expected), ctx));
}
bool unevaluated(const char* input) {
    Context ctx;
    const auto e = parse_text(input);
    return equal(evaluate(e, ctx), evaluate(e, ctx)) && evaluate(e, ctx)->has_head(e->head()->name());
}
}  // namespace

TEST_CASE("Linear algebra: structural operations on any entries") {
    CHECK(same("Transpose[{{a, b, c}, {d, e, f}}]", "{{a, d}, {b, e}, {c, f}}"));
    CHECK(same("Trace[{{a, b}, {c, d}}]", "a + d"));
    CHECK(same("IdentityMatrix[3]", "{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}"));
    CHECK(unevaluated("Trace[{{a, b}}]"));  // not square
    CHECK(unevaluated("Transpose[{{a, b}, {c}}]"));  // ragged
}

TEST_CASE("Linear algebra: exact numeric matrices") {
    CHECK(same("Det[{{1, 2}, {3, 4}}]", "-2"));
    CHECK(same("Det[{{2, 0, 1}, {1, 3, 2}, {1, 1, 1}}]", "0"));
    CHECK(same("Det[{{0, 1}, {1, 0}}]", "-1"));  // row swap changes the sign
    CHECK(same("Det[{{1/2, 1/3}, {1/4, 1/5}}]", "1/60"));
    CHECK(same("Inverse[{{1, 2}, {3, 4}}]", "{{-2, 1}, {3/2, -1/2}}"));
    CHECK(same("Inverse[{{2, 0, 0}, {0, 0, 1}, {0, 1, 0}}] . {{2, 0, 0}, {0, 0, 1}, {0, 1, 0}}",
               "IdentityMatrix[3]"));
    CHECK(unevaluated("Inverse[{{1, 2}, {2, 4}}]"));  // singular
    CHECK(same("Rank[{{1, 2, 3}, {2, 4, 6}, {1, 0, 1}}]", "2"));
    CHECK(same("RowReduce[{{1, 2, 3}, {4, 5, 6}}]", "{{1, 0, -1}, {0, 1, 2}}"));
    CHECK(same("LinearSolve[{{2, 1}, {1, 3}}, {3, 5}]", "{4/5, 7/5}"));
    CHECK(unevaluated("LinearSolve[{{1, 1}, {1, 1}}, {1, 2}]"));  // inconsistent
    CHECK(unevaluated("LinearSolve[{{1, 1}}, {1}]"));             // not unique
    CHECK(same("CharPoly[{{2, 1}, {1, 2}}, x]", "3 - 4*x + x^2"));
    CHECK(same("CharPoly[{{1, 2, 0}, {0, 3, 0}, {0, 0, 5}}, x]", "Expand[(x - 1)*(x - 3)*(x - 5)]"));
}

TEST_CASE("Linear algebra: symbolic entries are left for the backend") {
    CHECK(unevaluated("Det[{{a, b}, {c, d}}]"));
    CHECK(unevaluated("Inverse[{{a, 1}, {0, 1}}]"));
    CHECK(unevaluated("CharPoly[{{a, 1}, {0, 1}}, x]"));
}
