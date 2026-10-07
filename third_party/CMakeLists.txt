# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

option(SYMATS_USE_GIAC "Enable Giac symbolic math engine backend" OFF)

if(SYMATS_USE_GIAC)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/giac/CMakeLists.txt")
    add_subdirectory(giac)
  else()
    add_library(third_party_giac INTERFACE)
    add_library(third_party::giac ALIAS third_party_giac)
    target_compile_definitions(third_party_giac INTERFACE SYMATS_USE_GIAC=1)
  endif()
else()
  add_library(third_party_giac INTERFACE)
  add_library(third_party::giac ALIAS third_party_giac)
endif()
