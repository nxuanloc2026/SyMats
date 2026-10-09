# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
# Shared numeric evaluation for the Boost backends (Boost.Math special functions).
add_library(symats_numeric_expr numeric_expr.cpp)
add_library(symats::numeric_expr ALIAS symats_numeric_expr)
target_include_directories(symats_numeric_expr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(symats_numeric_expr PUBLIC symats::core PRIVATE Boost::headers)
target_compile_features(symats_numeric_expr PUBLIC cxx_std_20)
