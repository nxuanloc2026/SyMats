# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

option(SYMATS_USE_GIAC "Use Giac backend for symbolic math" OFF)

add_library(giac_target INTERFACE)
add_library(third_party::giac ALIAS giac_target)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/giac/CMakeLists.txt")
  add_subdirectory(giac)
else()
  target_include_directories(giac_target INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/giac_stub>
  )
endif()
