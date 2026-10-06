# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_library(symats_giac_backend giac_backend.cpp conversion.cpp)
add_library(symats::giac_backend ALIAS symats_giac_backend)
target_include_directories(symats_giac_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(symats_giac_backend PUBLIC symats::core PRIVATE third_party::giac)
target_compile_features(symats_giac_backend PUBLIC cxx_std_20)

if(SYMATS_BUILD_TESTS)
  add_executable(giac_backend_tests
    "${PROJECT_SOURCE_DIR}/tests/test_main.cpp" test_giac_backend.cpp)
  target_include_directories(giac_backend_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(giac_backend_tests PRIVATE symats::giac_backend symats::convert)
  add_test(NAME giac-backend COMMAND giac_backend_tests)
endif()
