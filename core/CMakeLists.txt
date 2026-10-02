# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_core
  src/integer.cpp
  src/rational.cpp
  src/expr.cpp
  src/canonical.cpp
  src/print.cpp
  src/pattern.cpp
  src/backend.cpp
  src/eval.cpp
  src/session.cpp
)
add_library(symats::core ALIAS symats_core)

target_include_directories(symats_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(symats_core PUBLIC cxx_std_20)
