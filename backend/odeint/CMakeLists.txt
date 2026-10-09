# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
# Boost.Odeint and Boost.Math are header-only; only this backend may include them.
find_package(Boost 1.71 REQUIRED)

add_library(symats_odeint_backend
  odeint_backend.cpp
)
add_library(symats::odeint_backend ALIAS symats_odeint_backend)
target_include_directories(symats_odeint_backend PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(symats_odeint_backend
  PUBLIC symats::core
  PRIVATE Boost::headers
)
target_compile_features(symats_odeint_backend PUBLIC cxx_std_20)
# uBLAS type checks make Debug Rosenbrock steps orders of magnitude slower.
target_compile_definitions(symats_odeint_backend PRIVATE BOOST_UBLAS_NDEBUG)

if(SYMATS_BUILD_TESTS)
  add_executable(odeint_backend_tests
    "${PROJECT_SOURCE_DIR}/tests/test_main.cpp" test_odeint_backend.cpp)
  target_include_directories(odeint_backend_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(odeint_backend_tests PRIVATE symats::odeint_backend symats::convert)
  add_test(NAME odeint-backend COMMAND odeint_backend_tests)
endif()
