// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// symats-cli: v0.1 demo of the expression core. A real read-eval-print loop
// arrives once the text parser (convert/) exists.
#include <iostream>

#include "symats/expr.h"

int main() {
    using namespace symats;
    const Ex x = sym("x"), y = sym("y");

    struct Demo {
        const char* input;
        Ex result;
    };
    const Demo demos[] = {
        {"x + x", x + x},
        {"2 + x + 3", Ex(2) + x + 3},
        {"x * x * y / x", x * x * y / x},
        {"(x + 1) - (x + 1)", (x + 1) - (x + 1)},
        {"(2x)^2", pow(2 * x, 2)},
        {"8^(2/3)", pow(8, frac(2, 3))},
        {"sqrt(2) * sqrt(2)", pow(2, frac(1, 2)) * pow(2, frac(1, 2))},
        {"2^100", pow(Ex(2), 100)},
        {"integral of x^2 dx", make_normal("Integrate", {pow(x, 2).ptr(), x.ptr()})},
    };

    std::cout << "Symats 0.1 - expression core demo\n\n";
    for (const auto& d : demos) std::cout << "  " << d.input << "\n    => " << d.result.str() << "\n";
    return 0;
}
