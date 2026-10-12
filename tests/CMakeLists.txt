# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_executable(symats_tests
  test_main.cpp
  test_integer.cpp
  test_rational.cpp
  test_expr.cpp
  test_text.cpp
  test_mathjson.cpp
  test_edge_cases.cpp
  test_backend_comparison.cpp
  test_pattern.cpp
  test_eval.cpp
  test_calculus.cpp
  test_session.cpp
  test_operators.cpp
  test_mathics_semantics.cpp
  test_bridge.cpp
  test_nintegrate.cpp
  ../backend/odeint/test_odeint_backend.cpp
)
target_include_directories(symats_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(symats_tests PRIVATE symats::core symats::convert symats::bridge symats::backend_odeint)
add_test(NAME symats_tests COMMAND symats_tests)
