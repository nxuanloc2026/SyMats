# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(backend_sundials
  sundials/sundials_backend.cpp
)
add_library(symats::backend_sundials ALIAS backend_sundials)

target_include_directories(backend_sundials PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)
target_link_libraries(backend_sundials PUBLIC symats::core)
target_compile_features(backend_sundials PUBLIC cxx_std_20)
