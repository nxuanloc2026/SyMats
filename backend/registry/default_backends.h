// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <string>
#include <vector>

#include "symats/backend.h"

namespace symats {

// Registers every backend this build includes (CMake options SYMATS_USE_GIAC,
// SYMATS_USE_SUNDIALS, SYMATS_USE_BOOST), in fallback order: Giac for symbolic
// operations; SUNDIALS, then Boost.Odeint for NDSolve; Boost quadrature for NIntegrate.
void install_default_backends(BackendRegistry& registry);

// Names of the backends install_default_backends adds, e.g. {"giac", "odeint"}.
std::vector<std::string> default_backend_names();

}  // namespace symats
