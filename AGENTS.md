# AGENTS.md — rules for every AI agent working on Symats

Applies to Claude Code, OpenAI Codex, GitHub Copilot coding agent, and Google Jules.
Read this file in full before starting any task.

## Project

Symats is a free, open-source symbolic mathematics program. The user writes math
**either in real 2-D notation** (integral signs with limit slots, fractions, roots,
sums, derivatives, matrices) **or in plain text** (`integrate(x^2, x, 0, 1)`), and
can switch between the two at any time. The engine computes exact symbolic answers,
solves multivariable differential equations and integrals, displays results as
math, and plots equations — including time-varying (animated) ones.

- Engine: C++20 library, independent of any UI.
- Both input modes produce the **same expression tree**; the tree is the source of truth.

## Core requirements

1. **Interchangeable input (symbolic ⇄ plain text).**
   - Every cell can be shown/edited as 2-D math (MathLive, MIT license) or as plain text.
   - A toggle converts the current cell losslessly in either direction:
     2-D → MathJSON → Expr → text, and text → parser → Expr → MathJSON → 2-D.
   - Mixed use is allowed in one notebook (some cells symbolic, some text).
   - Round-trip must be stable: converting text → 2-D → text yields an equivalent
     expression (formatting may be normalized).
2. **Symbolic matrix input.**
   - Matrix template with editable cells; add/remove rows and columns; entries may be
     any symbolic expression. Vectors (row/column) and block matrices.
   - Plain-text equivalent: `[[a, b], [c, d]]`.
   - Operations: +, −, ×, scalar ×, transpose, det, inverse, rank, trace, rref,
     eigenvalues/eigenvectors, characteristic polynomial, matrix exponential, solve A·x = b.
3. **Integration (single and multivariable).**
   - Indefinite and definite integrals; multiple/iterated integrals (∬, ∭) with
     variable limits; line and surface integrals (later).
   - Strategy: table + rule-based methods (substitution, parts, partial fractions,
     trig/hyperbolic rules), Risch algorithm for rational/elementary cases,
     then **numeric fallback** (adaptive quadrature, cubature for multiple integrals)
     when no closed form is found. Always say whether a result is exact or numeric.
4. **Differential equations.**
   - ODEs: first/second-order and higher, linear with constant coefficients,
     separable, exact, Bernoulli, Cauchy–Euler, Laplace-transform method;
     initial/boundary value problems.
   - **Systems of ODEs** (multiple unknown functions): linear systems via the matrix
     exponential / eigen-decomposition; nonlinear systems numerically.
   - **PDEs (multivariable)**: first-order linear via characteristics; classical
     second-order (heat, wave, Laplace) on simple domains via separation of variables
     and Fourier series. Anything else → numeric solver (method of lines, finite
     differences).
   - Numeric ODE solvers: RK45 (adaptive), stiff solver (BDF/Rosenbrock).
   - Notation: y′, y″, dy/dx, ∂u/∂t, ∂²u/∂x² all available in the 2-D editor and as text
     (`D(y(x), x)`, `D(u(x,t), t)`).
   - Results feed directly into plotting (solution curves, phase portraits, animated PDE solutions).
5. **Plotting, including time-varying plots** — see "Plotting requirements".

Scope note: general closed-form solutions of integrals and PDEs do not always exist.
When the symbolic method fails, fall back to numeric methods and label the result.

## Layers (each depends on the ones below)

```
6. Plotting       — static + animated (time-varying); 2-D, parametric, polar, implicit, 3-D
5. Notebook + editors — 2-D math editor and plain-text editor, toggle per cell; matrix template
4. Math functions — Integrate (multi), D, Series, Solve, DSolve (ODE/PDE/systems),
                    NDSolve, NIntegrate, linear algebra, Simplify, Factor
3. Evaluator + rules — pattern matching, definitions, attributes
2. Converter      — MathJSON ⇄ Expr ⇄ plain text (parser + printer); LaTeX printer
1. Expression core — immutable shared expression trees, canonical form
0. Build + CI + tests
```

## Layout

```
core/         libsymats_core — expressions, canonical form, evaluator, math functions
core/numeric/ compile Expr → fast numeric function; adaptive sampling for plots
convert/      MathJSON ⇄ Expr, text parser, LaTeX printer
app/          desktop app: MathLive editor, notebook, plot rendering (web front end)
bridge/       connects the C++ engine to the app (Tauri/Qt WebEngine or WebAssembly)
cli/          symats-cli command-line program
tests/        unit, round-trip and plot-accuracy tests
docs/         design notes, expression-format spec (docs/EXPR_SPEC.md)
```

`core/` must never depend on `app/`, `bridge/`, or any UI library.
All agents must use the node names defined in `docs/EXPR_SPEC.md`.

## Lanes — who owns what

Each agent works only in its own lane unless the issue says otherwise.
If a change outside your lane is unavoidable, keep it minimal and explain it in the PR.

| Agent   | Lane                                                                                  | Issue label     |
|---------|---------------------------------------------------------------------------------------|-----------------|
| Claude  | `core/` — expression core, evaluator, integration, DSolve (ODE/PDE/systems), linear algebra; `core/numeric/` — NIntegrate, NDSolve, plot sampling | `agent:claude`  |
| Codex   | `app/` — 2-D editor, plain-text editor + toggle, matrix template, notebook, static/animated plot rendering; `convert/` — MathJSON ⇄ Expr ⇄ text | `agent:codex`   |
| Copilot | `bridge/`, `cli/`, CMake, CI, installers, small bug fixes                              | `agent:copilot` |
| Jules   | `tests/` — unit, symbolic⇄text round-trip, integration/DE checks (verify by differentiation/substitution), plot-accuracy tests | `jules`         |

## Plotting requirements

- Plot types: y = f(x); parametric (x(t), y(t)); polar r(θ); implicit F(x, y) = 0;
  3-D surfaces z = f(x, y); contour plots. Multiple curves per plot.
- The engine compiles a symbolic expression into a fast numeric function, then samples
  adaptively: more points where curvature is high; detect discontinuities and asymptotes
  (e.g. tan x, 1/x) and break the curve instead of drawing vertical lines.
- Implicit curves: marching squares. Surfaces: triangle mesh.
- The front end renders interactively (zoom, pan, hover values) and supports parameter
  sliders (e.g. plot sin(a·x) with a slider for a).
- A plot can be created from any equation in the editor (Plot button / template).

### Time-varying (animated) plots

- Any plot whose expression contains a time parameter (default `t`, user-selectable)
  can be animated: play / pause / step / loop, speed control, and a time slider.
- Examples: traveling wave y = sin(x − t); rotating parametric curve; heat-equation
  solution u(x, t); 3-D surface z = sin(√(x²+y²) − t); trajectories of ODE systems
  traced over time; vector/phase fields with moving points.
- The engine pre-samples frames (or samples on demand) using the compiled numeric
  function; the front end renders frames smoothly (target 30–60 fps).
- Export animation as GIF/MP4 (later).
- Text form: `animate(plot(sin(x - t), x, 0, 2*pi), t, 0, 10)`.

## Workflow

1. One GitHub issue = one branch = one pull request. Keep PRs small (< ~400 lines).
2. Never push directly to `main`. Open a PR; a human merges.
3. The project must build and all tests must pass before a PR is ready for review.
4. Do not start an issue that is already assigned or has an open PR.
5. If the issue is unclear, comment on the issue with questions instead of guessing.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Code rules

- C++20, CMake ≥ 3.20. No compiler-specific extensions.
- Every source file starts with:
  ```cpp
  // SPDX-License-Identifier: GPL-3.0-or-later
  // Copyright (c) 2026 Loc Ngo and Symats contributors
  ```
- Namespace: `symats`. Headers in `core/include/symats/`.
- Expressions are immutable and shared (`std::shared_ptr<const Expr>`).
- Arbitrary precision: v0.1 uses the built-in `symats::Integer`/`Rational` (zero
  dependencies). Do not change their public interface; a GMP backend will replace the
  internals later. For new heavy machinery (polynomial factoring, multiprecision
  floats) use FLINT/MPFR rather than reimplementing them.
- Tests use the in-repo harness `tests/test.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `CHECK_THROWS`).
- Every new function needs tests.

## Legal rules (important)

- Do NOT copy code, documentation text, examples, or images from Mathematica/Wolfram,
  MATLAB/MathWorks, or any proprietary software.
- Implement algorithms from textbooks and published papers; cite the source in a code comment.
- Test expected values must come from mathematics, textbooks, or open-source systems
  (SymPy, Maxima, PARI/GP) — never from proprietary software output.
- Only add dependencies whose licenses are compatible with GPL-3.0-or-later.
