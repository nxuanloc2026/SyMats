# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_library(symats_backend_odeint
  odeint_backend.cpp
)
add_library(symats::backend::odeint ALIAS symats_backend_odeint)

target_include_directories(symats_backend_odeint PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(symats_backend_odeint PUBLIC symats::core)
target_compile_features(symats_backend_odeint PUBLIC cxx_std_20)
