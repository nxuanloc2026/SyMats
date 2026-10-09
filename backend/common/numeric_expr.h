// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Function table for numeric backends: the core elementary functions plus the
// Boost.Math special functions of docs/EXPR_SPEC.md §3.3.
#pragma once

#include "symats/numeric.h"

namespace symats::numeric {

const Functions& special_functions();

}  // namespace symats::numeric
