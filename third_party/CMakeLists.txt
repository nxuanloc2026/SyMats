# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

if(SYMATS_USE_GIAC)
set(GIAC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/giac/src")
if(NOT EXISTS "${GIAC_SOURCE_DIR}/gen.cc")
  message(FATAL_ERROR "Giac submodule is missing; run git submodule update --init")
endif()

find_path(GIAC_GMP_INCLUDE_DIR gmp.h REQUIRED)
find_path(GIAC_MPFR_INCLUDE_DIR mpfr.h REQUIRED)
find_library(GIAC_GMP_LIBRARY NAMES gmp libgmp REQUIRED)
find_library(GIAC_GMPXX_LIBRARY NAMES gmpxx libgmpxx REQUIRED)
find_library(GIAC_MPFR_LIBRARY NAMES mpfr libmpfr REQUIRED)

if(WIN32)
  find_program(GIAC_FLEX NAMES win_flex flex REQUIRED)
  find_program(GIAC_BISON NAMES win_bison bison REQUIRED)
else()
  find_program(GIAC_FLEX NAMES flex REQUIRED)
  find_program(GIAC_BISON NAMES bison REQUIRED)
endif()

set(GIAC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GIAC_GENERATED_DIR}")
add_custom_command(
  OUTPUT "${GIAC_GENERATED_DIR}/input_parser.cc" "${GIAC_GENERATED_DIR}/input_parser.h"
  COMMAND "${GIAC_BISON}" -y -d -p giac_yy
          --defines=input_parser.h -o input_parser.cc "${GIAC_SOURCE_DIR}/input_parser.yy"
  DEPENDS "${GIAC_SOURCE_DIR}/input_parser.yy"
  WORKING_DIRECTORY "${GIAC_GENERATED_DIR}"
  VERBATIM)
add_custom_command(
  OUTPUT "${GIAC_GENERATED_DIR}/input_lexer.cc" "${GIAC_GENERATED_DIR}/lexer.h"
  COMMAND "${GIAC_FLEX}" -o input_lexer.cc "${GIAC_SOURCE_DIR}/input_lexer.ll"
  DEPENDS "${GIAC_SOURCE_DIR}/input_lexer.ll"
  WORKING_DIRECTORY "${GIAC_GENERATED_DIR}"
  VERBATIM)

# Source list follows Giac's own src/Makefile.vcc. Keep UI and optional-language
# components out of this static library; the bridge calls Giac through gen.
set(GIAC_SOURCES
  sym2poly.cc gausspol.cc threaded.cc maple.cc ti89.cc mathml.cc moyal.cc
  misc.cc permu.cc quater.cc desolve.cc symbolic.cc index.cc modpoly.cc
  modfactor.cc ezgcd.cc derive.cc solve.cc intg.cc intgab.cc risch.cc
  lin.cc series.cc subst.cc vecteur.cc sparse.cc csturm.cc tex.cc
  ifactor.cc alg_ext.cc gauss.cc isom.cc help.cc plot.cc plot3d.cc
  rpn.cc prog.cc pari.cc cocoa.cc optimization.cc signalprocessing.cc
  lpsolve.cc graphe.cc graphtheory.cc markup.cc TmpLESystemSolver.C
  TmpFGLM.C unary.cc usual.cc identificateur.cc gen.cc tinymt32.cc
  global.cc first.cc)
list(TRANSFORM GIAC_SOURCES PREPEND "${GIAC_SOURCE_DIR}/")
add_library(symats_giac STATIC
  ${GIAC_SOURCES}
  "${GIAC_GENERATED_DIR}/input_parser.cc"
  "${GIAC_GENERATED_DIR}/input_lexer.cc")
add_library(third_party::giac ALIAS symats_giac)
target_compile_features(symats_giac PUBLIC cxx_std_20)
target_include_directories(symats_giac PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/giac" "${GIAC_SOURCE_DIR}" "${GIAC_GENERATED_DIR}"
  "${GIAC_GMP_INCLUDE_DIR}" "${GIAC_MPFR_INCLUDE_DIR}")
target_compile_definitions(symats_giac PUBLIC
  HAVE_GMPXX_H HAVE_LIBMPFR VERSION="1.9.0.57")
target_compile_definitions(symats_giac PRIVATE IN_GIAC)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(symats_giac PRIVATE HAVE_GETTEXT HAVE_PWD_H HAVE_SYS_TYPES_H)
endif()
if(MSVC)
  target_compile_definitions(symats_giac PUBLIC
    VISUALC MS_SMART HAVE_NO_SYS_TIMES_H _USE_MATH_DEFINES YY_NO_UNISTD_H)
  target_compile_definitions(symats_giac PRIVATE
    HAVE_NO_PWD_H HAVE_NO_SYS_RESOURCE_WAIT_H HAVE_NO_HOME_DIRECTORY)
  target_compile_options(symats_giac PRIVATE /W0)
endif()
target_link_libraries(symats_giac PUBLIC
  "${GIAC_GMPXX_LIBRARY}" "${GIAC_GMP_LIBRARY}" "${GIAC_MPFR_LIBRARY}")

if(SYMATS_BUILD_TESTS)
  add_executable(giac_smoke giac_smoke.cpp)
  target_link_libraries(giac_smoke PRIVATE third_party::giac)
  add_test(NAME giac-smoke COMMAND giac_smoke)
endif()
  endif()

  if(SYMATS_USE_SUNDIALS)
    find_path(SUNDIALS_INCLUDE_DIR cvode/cvode.h REQUIRED)
    find_library(SUNDIALS_CVODE_LIBRARY NAMES sundials_cvode REQUIRED)
    find_library(SUNDIALS_NVECSERIAL_LIBRARY NAMES sundials_nvecserial REQUIRED)
    find_library(SUNDIALS_SUNMATRIXDENSE_LIBRARY NAMES sundials_sunmatrixdense REQUIRED)
    find_library(SUNDIALS_SUNLINSOLDENSE_LIBRARY NAMES sundials_sunlinsoldense REQUIRED)
    find_library(SUNDIALS_CORE_LIBRARY NAMES sundials_sundials_core sundials_core REQUIRED)

    add_library(symats_sundials INTERFACE)
    add_library(third_party::sundials ALIAS symats_sundials)
    target_include_directories(symats_sundials INTERFACE "${SUNDIALS_INCLUDE_DIR}")
    target_link_libraries(symats_sundials INTERFACE
      "${SUNDIALS_CVODE_LIBRARY}"
      "${SUNDIALS_NVECSERIAL_LIBRARY}"
      "${SUNDIALS_SUNMATRIXDENSE_LIBRARY}"
      "${SUNDIALS_SUNLINSOLDENSE_LIBRARY}"
      "${SUNDIALS_CORE_LIBRARY}")
  endif()
