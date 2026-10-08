// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <set>

#include "giac.h"
#include "symats/expr.h"

namespace symats::giac_detail {
using GiacHead = const giac::unary_function_ptr* const*;
GiacHead find_head(std::string_view name);
giac::gen to_giac(const ExprPtr& expr, bool for_evaluation = true, const ExprPtr& ode_variable = {});
std::optional<ExprPtr> from_giac(const giac::gen& value, const std::set<std::string>& reserved = {});
giac::gen sequence(const ExprList& args, const ExprPtr& ode_variable = {});
giac::gen solve_system(const ExprPtr& expr, giac::context* context);
}  // namespace symats::giac_detail
