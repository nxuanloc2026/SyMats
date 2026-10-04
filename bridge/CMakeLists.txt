# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_library(symats_bridge STATIC src/api.cpp)
add_library(symats::bridge ALIAS symats_bridge)
target_include_directories(symats_bridge PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(symats_bridge PUBLIC symats::core symats::convert)
target_compile_features(symats_bridge PUBLIC cxx_std_20)

if(EMSCRIPTEN)
  add_executable(symats_wasm src/api.cpp)
  target_include_directories(symats_wasm PRIVATE include)
  target_link_libraries(symats_wasm PRIVATE symats::core symats::convert)
  target_link_options(symats_wasm PRIVATE
    "--no-entry" "-sMODULARIZE=1" "-sEXPORT_ES6=1"
    "-sENVIRONMENT=worker" "-sALLOW_MEMORY_GROWTH=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "-sEXPORTED_FUNCTIONS=['_symats_run_text','_symats_run_mathjson','_symats_text_to_mathjson','_symats_mathjson_to_text','_symats_user_symbols','_symats_restart']")
  set_target_properties(symats_wasm PROPERTIES SUFFIX ".js")
endif()
