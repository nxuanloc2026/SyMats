# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_library(symats_bridge
    src/bridge.cpp
)

target_include_directories(symats_bridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(symats_bridge PRIVATE symats::core symats::convert)
add_library(symats::bridge ALIAS symats_bridge)
