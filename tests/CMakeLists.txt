# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_executable(symats_tests
  test_main.cpp
  test_integer.cpp
  test_rational.cpp
  test_expr.cpp
  test_text.cpp
  test_edge_cases.cpp
)
target_link_libraries(symats_tests PRIVATE symats::core symats::convert)
add_test(NAME symats_tests COMMAND symats_tests)
