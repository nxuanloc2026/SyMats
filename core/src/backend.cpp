// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/backend.h"

namespace symats {
namespace {

MathBackendPtr& global_backend_instance() {
    static MathBackendPtr backend = nullptr;
    return backend;
}

}  // namespace

void register_math_backend(MathBackendPtr backend) {
    global_backend_instance() = std::move(backend);
}

MathBackendPtr get_math_backend() {
    return global_backend_instance();
}

}  // namespace symats
