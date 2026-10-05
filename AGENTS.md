# AGENTS.md — rules for every AI agent working on Symats

Applies to Claude Code, OpenAI Codex, GitHub Copilot coding agent, Google Jules,
and collaborators assigned tasks on the board.
Read this file in full before starting any task.

> **Before every session, follow [docs/COORDINATION.md](docs/COORDINATION.md):**
> read `coordination/STATUS.md`, `coordination/BOARD.md`, and your handoff file in
> `coordination/handoff/`; claim a task; checkpoint (commit + handoff update) at least
> every ~30 minutes, because any agent can run out of usage without warning. If another
> agent is out or its task is stale (> 12 h), the backup matrix in COORDINATION.md says
> who continues and how.

## Project

Symats is a free, open-source symbolic mathematics program — **Mathematica-like, with
a lighter, bare UI**. It is a **notebook with a live kernel**: the user writes lines of
code in cells, defines variables and functions, executes cells, and later cells see
everything defined earlier (like Mathematica notebooks and the MATLAB command
window/scripts). Input is **plain text or real 2-D notation** (integral signs with limit
slots, fractions, roots, sums, derivatives, matrices), switchable per cell. The engine
computes exact symbolic answers, solves multivariable differential equations and
integrals, displays results as math, and plots — including time-varying (animated) plots.

- Engine: C++20 library, independent of any UI.
- Both input modes produce the **same expression tree**; the tree is the source of truth.

## Notebook model (decided 2026-10-02) — read before touching app/, convert/, or the kernel

**Text syntax.** Built-in functions and symbols have capitalized, case-sensitive names.
Calls use square brackets (`Sin[x]`, `Integrate[x^2, x]`, `f[x_] := Sin[x]`), lists and
matrices use curly braces (`{a, b}`, `{{a, b}, {c, d}}`), and parentheses group arithmetic.
`f(x)` means multiplication; `f[x]` calls `f`. The parser and printer use this notation
throughout the notebook, CLI, scripts, and tests. Only the Tier 1 operators below are
part of the public text syntax.

**Kernel.** One `symats::Session` (core/session.h) per open notebook. All cells share it:
`a = 5` in cell 1 is visible in cell 7. *Restart kernel* clears all definitions.

**Cells.** A notebook is a vertical list of cells. Kinds: `input` (code), `text`
(notes, Markdown), later `section`. An input cell holds **one or more statements**:
- statements are separated by newlines or `;`; a newline inside open brackets, or after a
  trailing binary operator, continues the statement;
- a statement ending in `;` is evaluated but its output is **not shown** (MATLAB/Mathematica);
- statements run top to bottom; every shown result gets an output line
  `Out[n]` (numbering is per session, like Mathematica `In[n]:=` / `Out[n]=`);
- `Null` results (e.g. from `f[x_] := x^2`) are not shown;
- an error in one statement is shown inline under that statement; later statements in the
  cell still run; the kernel survives.

**Executing.** `Shift+Enter` runs the current cell and moves to the next (creating one if
needed). `Enter` inserts a newline. Also: *Run all*, *Abort* (stops a long evaluation),
*Restart kernel*. A running cell shows a subtle busy marker; evaluation runs off the UI thread.

**History.** `%` = last output, `%%` = the one before, `%5` / `Out[5]` = output 5.

**Workspace (MATLAB-style, optional).** A collapsible side panel listing user-defined
symbols and their values/definitions (from `Session::user_symbols()`). Hidden by default.

**Scripts.** A notebook can be exported to / imported from a plain `.sym` text file (one
statement per line, cells separated by blank-line `(* --- *)` markers), and `symats-cli
file.sym` runs a script like a MATLAB `.m` file. Notebook file format: `.symnb` (JSON).

**Bare UI — rules for app/.**
- White (or system dark) page, content column ~ 800 px, no hero header, no cards, no
  explanatory sentences, no colored buttons. The notebook *is* the page.
- Each cell: input in a monospace font with a thin left gutter showing `In[n]` faintly;
  output directly below, rendered as math (KaTeX), with `Out[n]` faintly in the gutter.
- A thin cell bracket on hover only; controls (math/text toggle, delete, move) appear on
  hover or via keyboard, never permanently.
- One minimal top bar: notebook name, ▶ run, ■ abort, ⟳ restart, workspace toggle,
  menu (…). Nothing else.
- 2-D math input: MathLive, entered per cell via toggle or shortcut (`Ctrl+M`); palette of
  templates (∫, Σ, matrix, fraction) opens on `\` or a hover icon — not shown permanently.
- Plots render inline under the statement that produced them.
- Keyboard-first: everything reachable without the mouse.

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

## Text operators

Precedence and node names: docs/EXPR_SPEC.md §3.12.

- **Tier 1:** square bracket calls, capitalized built-ins, `+ - * / ^`, implicit
  multiplication, `=`, `:=`, `==`, `!=`, `< <= > >=`, `->`, `/.`, patterns
  `x_` / `x_Integer`, `;`, `%` / `%%` / `%n`, lists and matrices with `{ }`,
  `.` (Dot), `'` (derivative: `y'[t]`), `&&`, `||`, prefix `!` and postfix `!`.
  All future parser, editor, CLI, and script tasks implement this syntax.

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
   - Plain-text equivalent: `{{a, b}, {c, d}}`.
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
     (`D[y[x], x]`, `D[u[x, t], t]`).
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
| Prince (`princearwan-code`) | Scoped owner of the combined T-033 notebook workflow across `convert/`, `core/` matching, `cli/`, `bridge/`, and `app/`; see [issue #17](https://github.com/nxuanloc2026/Symats/issues/17) | — |

## Plotting requirements

- Plot types: `y = f[x]`; parametric `(x[t], y[t])`; polar `r[θ]`;
  implicit `F[x, y] == 0`; 3-D surfaces `z = f[x, y]`; contour plots.
  Multiple curves per plot.
- The engine compiles a symbolic expression into a fast numeric function, then samples
  adaptively: more points where curvature is high; detect discontinuities and asymptotes
  (e.g. tan x, 1/x) and break the curve instead of drawing vertical lines.
- Implicit curves: marching squares. Surfaces: triangle mesh.
- The front end renders interactively (zoom, pan, hover values) and supports parameter
  sliders (e.g. `Plot[Sin[a*x], {x, 0, 2*Pi}]` with a slider for `a`).
- A plot can be created from any equation in the editor (Plot button / template).

### Time-varying (animated) plots

- Any plot whose expression contains a time parameter (default `t`, user-selectable)
  can be animated: play / pause / step / loop, speed control, and a time slider.
- Examples: traveling wave `y = Sin[x - t]`; rotating parametric curve; heat-equation
  solution `u[x, t]`; 3-D surface `z = Sin[Sqrt[x^2+y^2] - t]`; trajectories of ODE systems
  traced over time; vector/phase fields with moving points.
- The engine pre-samples frames (or samples on demand) using the compiled numeric
  function; the front end renders frames smoothly (target 30–60 fps).
- Export animation as GIF/MP4 (later).
- Text form: `Animate[Plot[Sin[x - t], {x, 0, 2*Pi}], {t, 0, 10}]`.

## Workflow

0. `coordination/BOARD.md` is the task list (mirror tasks as GitHub issues once the repo is on GitHub).
1. One task = one branch = one pull request. Keep PRs small (< ~400 lines).
   T-033 is a tracking bundle: each of its seven retained task IDs is one
   implementation slice with its own reviewable branch and PR, all linked to
   [issue #17](https://github.com/nxuanloc2026/Symats/issues/17).
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
  Rubi rules and test suite (MIT), **xeus** (BSD-3, Jupyter kernel), **CodeMirror 6** (MIT,
  cell editor), **Boost.Odeint / Boost.Math** (BSL-1.0), **SymEngine** (MIT, reserve backend),
  **Emscripten** (MIT), **STIX Two / Latin Modern Math** fonts (OFL), Numerica (MIT, see below).
  Reference/oracle only (GPL, may be read and translated, not linked): **Mathics3** (evaluator
  semantics, built-in behavior, doc-tests), **Maxima** (second test oracle next to SymPy).
  Anything else needs Loc's approval.
- **Symbolica policy.**
  - The `symbolica` library itself is source-available, not open source: do **not** read,
    copy, port, paraphrase, or link to its source code, and do not depend on it.
  - Its spin-off crates **Numerica** (github.com/symbolica-dev/numerica) and **Graphica**
    (github.com/symbolica-dev/graphica) are **MIT-licensed**: they may be read and their
    algorithms ported to C++ with attribution (keep the MIT copyright notice in the ported
    file and list it in THIRD_PARTY_NOTICES.md). Verify the LICENSE file of the exact
    version you use first. Useful parts: error-tracking floats, dual numbers (automatic
    differentiation), Vegas Monte Carlo integration, rational reconstruction, finite fields.
  - Public documentation, blog posts, and lecture notes may be read for ideas and cited
    like a textbook; never copy their text or examples verbatim.
  - Never use Symbolica's output as expected values in tests (only mathematics, textbooks,
    and open-source systems).
