# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors

find_package(xeus-zmq QUIET)
find_package(xeus QUIET)
find_package(nlohmann_json QUIET)

add_executable(symats_kernel
  main.cpp
  symats_interpreter.cpp
)

target_link_libraries(symats_kernel PRIVATE symats::core symats::convert symats::default_backends)
target_compile_features(symats_kernel PRIVATE cxx_std_20)

if(xeus-zmq_FOUND OR xeus_FOUND)
  target_compile_definitions(symats_kernel PRIVATE SYMATS_HAS_XEUS)
  if(TARGET xeus-zmq)
    target_link_libraries(symats_kernel PRIVATE xeus-zmq)
  endif()
  if(TARGET xeus)
    target_link_libraries(symats_kernel PRIVATE xeus)
  endif()
  if(TARGET nlohmann_json)
    target_link_libraries(symats_kernel PRIVATE nlohmann_json)
  endif()
endif()

set(SYMATS_KERNEL_PATH "$<TARGET_FILE:symats_kernel>")
configure_file(kernel.json.in "${CMAKE_CURRENT_BINARY_DIR}/kernel.json" @ONLY)
