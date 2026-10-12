# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_library(symats_bridge STATIC
    src/bridge.cpp
    src/c_api.cpp
)

add_library(symats::bridge ALIAS symats_bridge)

target_include_directories(symats_bridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(symats_bridge
    PUBLIC
        symats::core
        symats::convert
)

target_compile_features(symats_bridge PUBLIC cxx_std_20)
