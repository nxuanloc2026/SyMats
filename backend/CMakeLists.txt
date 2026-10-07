# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_library(symats_backend
  giac/giac_backend.cpp
)

add_library(symats::backend ALIAS symats_backend)

target_include_directories(symats_backend PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(symats_backend PUBLIC
  symats::core
  third_party::giac
)
