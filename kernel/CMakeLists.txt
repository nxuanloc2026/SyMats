# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

add_executable(symats_kernel
  src/main.cpp
)

target_include_directories(symats_kernel PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(symats_kernel PRIVATE symats::core symats::convert)
target_compile_features(symats_kernel PRIVATE cxx_std_20)

find_package(xeus QUIET)
if(xeus_FOUND)
  find_package(xeus-zmq QUIET)
  if(xeus-zmq_FOUND)
    target_link_libraries(symats_kernel PRIVATE xeus xeus-zmq)
    target_compile_definitions(symats_kernel PRIVATE SYMATS_HAVE_XEUS=1)
  endif()
endif()
