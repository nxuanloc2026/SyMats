# AGENTS.md — rules for every AI agent working on Symats

Applies to Claude Code, OpenAI Codex, GitHub Copilot coding agent, and Google Jules.
Read this file in full before starting any task.

> **Before every session, follow [docs/COORDINATION.md](docs/COORDINATION.md):**
> read `coordination/STATUS.md`, `coordination/BOARD.md`, and your handoff file in
> `coordination/handoff/`; claim a task; checkpoint (commit + handoff update) at least
> every ~30 minutes, because any agent can run out of usage without warning. If another
> agent is out or its task is stale (> 12 h), the backup matrix in COORDINATION.md says
> who continues and how.

## Project

Symats is a free, open-source symbolic mathematics program. The user writes math
**either in real 2-D notation** (integral signs with limit slots, fractions, roots,
sums, derivatives, matrices) **or in plain text** (`integrate(x^2, x, 0, 1)`), and
can switch between the two at any time. The engine computes exact symbolic answers,
solves multivariable differential equations and integrals, displays results as
math, and plots equations — including time-varying (animated) ones.

- Engine: C++20 library, independent of any UI.
- Both input modes produce the **same expression tree**; the tree is the source of truth.

## Architecture decision: reuse proven libraries (decided 2026-10-02)

Symats does **not** reimplement heavy mathematics. It keeps its own expression core and
spec as the common format, and delegates to proven open-source libraries:

| Concern | Library (license) | Used for |
|---------|-------------------|----------|
| Common format | **Symats `Expr` + `docs/EXPR_SPEC.md`** (ours) | The only format passed between editor, converter, engine, backends, plots |
| Symbolic math backend | **Giac** (GPL-3.0-or-later) | Integrate, Limit, Series, Solve, Factor, Simplify, DSolve (ODEs, systems), Laplace, symbolic linear algebra |
| Polynomials / exact numbers | FLINT 3 incl. Arb (LGPL-3) | Fast polynomial arithmetic and factoring; rigorous numeric checks |
| Numerics | **SUNDIALS** (BSD-3) | NDSolve (CVODE stiff/non-stiff ODEs, IDA), PDEs by method of lines |
| | Boost.Math quadrature (Boost) | NIntegrate and numeric verification |
| 2-D math editor | **MathLive** + Compute Engine (MIT) | Symbolic input, MathJSON |
| Plots | **Plotly.js** (MIT) | 2-D/3-D plots, animation frames, sliders |

### Rules for backends (protect against backend bugs and abandonment)

1. **One door.** Only `backend/` may include Giac, SUNDIALS, or FLINT headers. Everything
   else calls the `symats::MathBackend` interface with `Expr` in and `Expr` out.
2. **Swappable.** Each operation goes through the interface, so a backend can be replaced
   per function (e.g. FLINT for `Factor`, Giac for `Integrate`) without touching other code.
3. **Pinned.** Third-party code lives in `third_party/` at a pinned, tested version
   (Giac via our own fork). Upgrades only in a dedicated PR with the full test suite green.
4. **Verified.** Every backend result is checked by Symats' own core before it is shown:
   integrals by differentiation (or numeric comparison), solutions by substitution,
   ODE solutions by substituting into the equation and conditions, inverses by A·A⁻¹ = I.
   Results carry a status: `verified`, `numeric`, or `unverified` — the UI shows it.
5. **Fallback chain:** backend → Symats-native method (if any) → numeric result, labeled.
6. **Native first for the basics.** `D` (derivatives) and `Expand` are implemented natively in
   `core/` — they are needed to verify backend results and must not depend on Giac.

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
   - Strategy: Giac backend (verified by differentiation), then **numeric fallback**
     (Boost quadrature; cubature for multiple integrals) when no closed form is found.
     Always say whether a result is exact or numeric.
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
   - Symbolic ODEs and systems: Giac backend, verified by substitution.
   - Numeric ODE/PDE solvers: SUNDIALS (CVODE for stiff and non-stiff ODEs; method of lines for PDEs).
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
4. Math functions — native: D, Expand, verification; via backend/: Integrate (multi),
                    Series, Solve, DSolve, NDSolve, NIntegrate, linear algebra, Simplify, Factor
3b. Backend bridge  — MathBackend interface; Giac / FLINT / SUNDIALS adapters (Expr ⇄ library)
3. Evaluator + rules — pattern matching, definitions, attributes; dispatches math heads to backends
2. Converter      — MathJSON ⇄ Expr ⇄ plain text (parser + printer); LaTeX printer
1. Expression core — immutable shared expression trees, canonical form
0. Build + CI + tests
```

## Layout

```
core/         libsymats_core — expressions, canonical form, evaluator, D, Expand, verification
core/numeric/ compile Expr → fast numeric function; adaptive sampling for plots
backend/      MathBackend interface + adapters: Giac, FLINT, SUNDIALS (the only place they are included)
third_party/  pinned third-party sources (giac fork as git submodule, etc.)
convert/      MathJSON ⇄ Expr, text parser, LaTeX printer
app/          desktop app: MathLive editor, notebook, Plotly rendering (web front end)
bridge/       connects the C++ engine to the app (Tauri/Qt WebEngine or WebAssembly)
cli/          symats-cli command-line program
tests/        unit, round-trip, backend-comparison and plot-accuracy tests
docs/         design notes, expression-format spec (docs/EXPR_SPEC.md)
```

`core/` must never depend on `backend/`, `app/`, `bridge/`, or any UI library.
`core/` defines the `MathBackend` interface; `backend/` implements it.
All agents must use the node names defined in `docs/EXPR_SPEC.md`.

## Lanes — who owns what

Each agent works only in its own lane unless the issue says otherwise.
If a change outside your lane is unavoidable, keep it minimal and explain it in the PR.

| Agent   | Lane                                                                                  | Issue label     |
|---------|---------------------------------------------------------------------------------------|-----------------|
| Claude  | `core/` — expression core, evaluator, D, Expand, result verification; `backend/` — MathBackend interface and Expr ⇄ Giac/FLINT/SUNDIALS conversion; `core/numeric/` — plot sampling | `agent:claude`  |
| Codex   | `app/` — MathLive editor, plain-text editor + toggle, matrix template, notebook, Plotly rendering (static/animated); `convert/` — MathJSON ⇄ Expr ⇄ text | `agent:codex`   |
| Copilot | `third_party/` + building Giac/FLINT/SUNDIALS with CMake on Windows/Linux, `bridge/`, `cli/`, CI, installers, small bug fixes | `agent:copilot` |
| Jules   | `tests/` — unit, symbolic⇄text round-trip, backend-comparison suites (Rubi integrals, SymPy cases), verification checks, plot-accuracy tests | `jules`         |

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

0. `coordination/BOARD.md` is the task list (mirror tasks as GitHub issues once the repo is on GitHub).
1. One task = one branch = one pull request. Keep PRs small (< ~400 lines).
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
  internals later. Heavy machinery comes from the approved backends (see "Architecture
  decision"), never reimplemented from scratch.
- Tests use the in-repo harness `tests/test.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `CHECK_THROWS`).
- Every new function needs tests.

## Legal rules (important)

- Do NOT copy code, documentation text, examples, or images from Mathematica/Wolfram,
  MATLAB/MathWorks, or any proprietary software.
- Implement algorithms from textbooks and published papers; cite the source in a code comment.
- Test expected values must come from mathematics, textbooks, or open-source systems
  (SymPy, Maxima, PARI/GP) — never from proprietary software output.
- Only add dependencies whose licenses are compatible with GPL-3.0-or-later.
  Approved: Giac (GPL-3+), FLINT/GMP/MPFR (LGPL-3), SUNDIALS (BSD-3), Boost (BSL-1.0),
  Eigen (MPL-2.0), MathLive/Compute Engine/Plotly.js/KaTeX/Tauri (MIT/Apache-2.0),
  Rubi rules and test suite (MIT). Anything else needs Loc's approval.
- Do not use Symbolica (not open source).
