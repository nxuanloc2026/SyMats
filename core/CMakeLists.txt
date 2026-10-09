# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_core
  src/integer.cpp
  src/rational.cpp
  src/expr.cpp
  src/canonical.cpp
  src/print.cpp
  src/pattern.cpp
  src/backend.cpp
  src/eval.cpp
  src/calculus.cpp
  src/verification.cpp
  src/session.cpp
)
add_library(symats::core ALIAS symats_core)

target_include_directories(symats_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(symats_core PUBLIC cxx_std_20)

if(SYMATS_USE_GMP)
  find_path(GMP_INCLUDE_DIR gmp.h)
  find_path(GMPXX_INCLUDE_DIR gmpxx.h)
  find_library(GMP_LIBRARY gmp)
  find_library(GMPXX_LIBRARY gmpxx)

  if(GMP_INCLUDE_DIR AND GMPXX_INCLUDE_DIR AND GMP_LIBRARY AND GMPXX_LIBRARY)
    target_compile_definitions(symats_core PUBLIC SYMATS_USE_GMP)
    target_include_directories(symats_core PUBLIC ${GMP_INCLUDE_DIR} ${GMPXX_INCLUDE_DIR})
    target_link_libraries(symats_core PUBLIC ${GMPXX_LIBRARY} ${GMP_LIBRARY})
  else()
    message(FATAL_ERROR "SYMATS_USE_GMP is ON but GMP/GMPXX libraries or headers were not found")
  endif()
endif()
