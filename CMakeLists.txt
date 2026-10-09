# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
cmake_minimum_required(VERSION 3.20)
project(symats VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYMATS_BUILD_TESTS "Build the Symats test suite" ON)
option(SYMATS_BUILD_CLI   "Build the symats-cli program" ON)
option(SYMATS_BUILD_KERNEL "Build the Jupyter kernel" ON)
option(SYMATS_USE_GMP     "Use GMP library for symats::Integer" OFF)
option(SYMATS_USE_GIAC    "Build the pinned Giac symbolic backend" OFF)
option(SYMATS_USE_SUNDIALS "Build the SUNDIALS CVODE numeric backend" OFF)

if(SYMATS_BUILD_TESTS)
  enable_testing()
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
add_subdirectory(convert)

if(SYMATS_USE_GIAC)
  add_subdirectory(third_party)
  add_subdirectory(backend/giac)
endif()

if(SYMATS_USE_SUNDIALS)
  if(NOT SYMATS_USE_GIAC)
    add_subdirectory(third_party)
  endif()
  add_subdirectory(backend/sundials)
endif()

if(SYMATS_BUILD_CLI)
  add_subdirectory(cli)
endif()

if(SYMATS_BUILD_KERNEL)
  add_subdirectory(kernel)
endif()

if(SYMATS_BUILD_TESTS)
  add_subdirectory(tests)
endif()
