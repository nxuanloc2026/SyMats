# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_sundials_backend
  sundials_backend.cpp
)
add_library(symats::sundials_backend ALIAS symats_sundials_backend)
target_include_directories(symats_sundials_backend PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(symats_sundials_backend PUBLIC
  symats::core
  third_party::sundials
)
target_compile_features(symats_sundials_backend PUBLIC cxx_std_20)

if(SYMATS_BUILD_TESTS)
  add_executable(sundials_backend_test
    "${PROJECT_SOURCE_DIR}/tests/test_main.cpp" test_sundials_backend.cpp)
  target_include_directories(sundials_backend_test PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(sundials_backend_test PRIVATE
    symats::sundials_backend
    symats::convert
  )
  add_test(NAME sundials-backend COMMAND sundials_backend_test)
endif()
