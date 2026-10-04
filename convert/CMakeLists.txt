# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_convert
  src/text.cpp
  src/mathjson.cpp
  src/cell.cpp
)
add_library(symats::convert ALIAS symats_convert)
target_include_directories(symats_convert PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(symats_convert PUBLIC symats::core)
target_compile_features(symats_convert PUBLIC cxx_std_20)
