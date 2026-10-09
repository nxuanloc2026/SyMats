# Symats

Open-source symbolic mathematics software written in C++.

Write math in real 2-D notation (integrals, fractions, matrices) or in plain text,
get exact symbolic answers, solve differential equations, and plot — including
time-varying plots. See [AGENTS.md](AGENTS.md) for the full goals and
[docs/EXPR_SPEC.md](docs/EXPR_SPEC.md) for the expression format.

Plain text uses square brackets for calls and capitalized built-ins:
`Sin[x]`, `Integrate[x^2, {x, 0, 1}]`, and `f[x_] := Sin[x]`.
Use curly braces for lists and matrices: `{{a, b}, {c, d}}`.

## Status

**v0.1 — expression core.** Exact big integers and fractions, symbolic expression
trees, and automatic canonical simplification:

```
x + x              => Times[2, x]
(x + 1) - (x + 1)  => 0
(2x)^2             => Times[4, Power[x, 2]]
8^(2/3)            => 4
Sqrt[2] * Sqrt[2]  => 2
```

Next: complete Tier 1 text operators and the notebook UI, then calculus.

## Build

Requirements: a C++20 compiler and CMake 3.20+. No other dependencies yet.

**Visual Studio 2022:** File → Open → Folder… → select this folder. Visual Studio
reads `CMakePresets.json`; choose the `windows-debug` preset, then Build → Build All.
Run `symats-cli.exe` or the tests from the Test Explorer.

**Command line:**

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure   # run tests
./build/cli/symats-cli                        # run demo (Windows: build\cli\Debug\symats-cli.exe)
```

With no arguments, `symats-cli` starts an interactive REPL. Enter canonical Tier 1
expressions such as `Sin[Pi/2]` or `x = 5`; enter `quit` or `exit` to leave. Passing
a `.sym` path runs the file as a script and prints each visible result as `Out[n]`.

## License

Symats is free software, licensed under the [GNU General Public License v3.0](LICENSE).
