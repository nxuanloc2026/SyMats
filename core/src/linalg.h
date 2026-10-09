// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

namespace symats {
class Context;

// Native exact linear algebra built-ins (eval.cpp calls this). Transpose and Trace work
// on any entries; Det, Inverse, Rank, RowReduce, LinearSolve and CharPoly only on
// matrices of exact numbers, so symbolic matrices still reach the Giac backend.
void install_linear_algebra(Context& ctx);
}  // namespace symats
