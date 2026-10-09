# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_quadrature_backend quadrature_backend.cpp vegas.cpp)
add_library(symats::quadrature_backend ALIAS symats_quadrature_backend)
target_include_directories(symats_quadrature_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(symats_quadrature_backend
  PUBLIC symats::core
  PRIVATE symats::numeric_expr Boost::headers
)
target_compile_features(symats_quadrature_backend PUBLIC cxx_std_20)

if(SYMATS_BUILD_TESTS)
  add_executable(quadrature_backend_tests
    "${PROJECT_SOURCE_DIR}/tests/test_main.cpp" test_quadrature_backend.cpp)
  target_include_directories(quadrature_backend_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(quadrature_backend_tests PRIVATE symats::quadrature_backend symats::convert)
  add_test(NAME quadrature-backend COMMAND quadrature_backend_tests)
endif()
