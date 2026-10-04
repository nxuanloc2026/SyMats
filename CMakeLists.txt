# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
cmake_minimum_required(VERSION 3.20)
project(symats VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYMATS_BUILD_TESTS "Build the Symats test suite" ON)
option(SYMATS_BUILD_CLI   "Build the symats-cli program" ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
add_subdirectory(convert)
add_subdirectory(bridge)

if(SYMATS_BUILD_CLI)
  add_subdirectory(cli)
endif()

if(SYMATS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
