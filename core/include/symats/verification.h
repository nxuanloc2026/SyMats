// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include "symats/expr.h"

namespace symats {

// Verify a backend result using Symats-native exact operations. Returns false
// when the operation has no sound native check or the result fails its check.
bool verify_backend_result(const ExprPtr& request, const ExprPtr& result);

}  // namespace symats
