// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <exception>
#include <iostream>

#include "test.h"

int main() {
    int crashed = 0;
    for (const auto& c : symats_test::registry()) {
        int before = symats_test::failures();
        try {
            c.fn();
        } catch (const std::exception& e) {
            ++crashed;
            std::cerr << "[" << c.name << "] threw: " << e.what() << "\n";
        }
        std::cout << (symats_test::failures() == before ? "  ok    " : "  FAIL  ") << c.name << "\n";
    }
    int total = static_cast<int>(symats_test::registry().size());
    int bad = symats_test::failures() + crashed;
    std::cout << "\n" << total << " test cases, " << symats_test::failures() << " failed checks, "
              << crashed << " exceptions\n";
    return bad == 0 ? 0 : 1;
}
