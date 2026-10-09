# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
# Always built: installs whichever optional backends this configuration includes.
add_library(symats_default_backends default_backends.cpp)
add_library(symats::default_backends ALIAS symats_default_backends)
target_include_directories(symats_default_backends PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(symats_default_backends PUBLIC symats::core)
target_compile_features(symats_default_backends PUBLIC cxx_std_20)
foreach(backend IN ITEMS giac sundials)
  string(TOUPPER "${backend}" upper)
  if(TARGET symats::${backend}_backend)
    target_link_libraries(symats_default_backends PRIVATE symats::${backend}_backend)
    target_compile_definitions(symats_default_backends PRIVATE SYMATS_HAVE_${upper})
  endif()
endforeach()
if(TARGET symats::odeint_backend AND TARGET symats::quadrature_backend)
  target_link_libraries(symats_default_backends PRIVATE
    symats::odeint_backend symats::quadrature_backend)
  target_compile_definitions(symats_default_backends PRIVATE SYMATS_HAVE_BOOST)
endif()

if(SYMATS_BUILD_TESTS)
  add_executable(default_backends_tests
    "${PROJECT_SOURCE_DIR}/tests/test_main.cpp" test_default_backends.cpp)
  target_include_directories(default_backends_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(default_backends_tests PRIVATE symats::default_backends symats::convert)
  if(TARGET symats::odeint_backend)
    target_compile_definitions(default_backends_tests PRIVATE SYMATS_TEST_BOOST)
  endif()
  if(TARGET symats::giac_backend)
    target_compile_definitions(default_backends_tests PRIVATE SYMATS_TEST_GIAC)
  endif()
  add_test(NAME default-backends COMMAND default_backends_tests)
endif()
