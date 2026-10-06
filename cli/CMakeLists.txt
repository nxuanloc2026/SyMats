# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
add_executable(symats-cli main.cpp)
target_link_libraries(symats-cli PRIVATE symats::core symats::convert)
