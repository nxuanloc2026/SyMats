// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "default_backends.h"

#include <memory>

#ifdef SYMATS_HAVE_GIAC
#include "giac_backend.h"
#endif
#ifdef SYMATS_HAVE_SUNDIALS
#include "sundials_backend.h"
#endif
#ifdef SYMATS_HAVE_BOOST
#include "odeint_backend.h"
#include "quadrature_backend.h"
#endif

namespace symats {
namespace {
std::vector<std::shared_ptr<MathBackend>> make_backends() {
    std::vector<std::shared_ptr<MathBackend>> backends;
#ifdef SYMATS_HAVE_GIAC
    backends.push_back(std::make_shared<GiacBackend>());
#endif
#ifdef SYMATS_HAVE_SUNDIALS
    backends.push_back(std::make_shared<SundialsBackend>());
#endif
#ifdef SYMATS_HAVE_BOOST
    backends.push_back(std::make_shared<OdeintBackend>());
    backends.push_back(std::make_shared<QuadratureBackend>());
#endif
    return backends;
}
}  // namespace

void install_default_backends(BackendRegistry& registry) {
    for (auto& backend : make_backends()) registry.add(std::move(backend));
}

std::vector<std::string> default_backend_names() {
    std::vector<std::string> names;
    for (const auto& backend : make_backends()) names.push_back(backend->name());
    return names;
}

}  // namespace symats
