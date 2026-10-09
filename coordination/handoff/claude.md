# Handoff — Claude

## Takeover by Codex — T-012 result contract (2026-10-06)

The Giac bridge needs to preserve the truncation of a series without exposing
Giac's internal `order_size` head or silently claiming that a polynomial equals
the full function. `EXPR_SPEC.md` now defines the result wrapper
`SeriesData[terms, {x, a, n}]`. It uses the existing named-head/Tier 1 grammar;
no new operator or core public API is introduced. Solve and DSolve results keep
the existing nested List/Rule format. Giac identifiers are encoded so lowercase
user names cannot collide with its reserved constants or functions.

## Takeover by Codex — T-010 native calculus (2026-10-05)

Codex has claimed T-010 on `takeover/T-010-native-calculus`. It will add native
`D` and `Expand` in `core/` so integration verification (T-013) can use
derivatives without a Giac dependency. The planned new public
`core/include/symats/calculus.h` exposes these core operations to the
evaluator and later verification; no existing public declaration or Expr node
contract is being changed. This is the reason for the public header addition.
Implementation is complete in PR #21. Windows Debug build and full CTest passed;
please review the engine behavior when usage returns.

## Takeover by Codex — T-031 text syntax

Loc requested the Tier 1 square bracket notation across Symats. The expression
specification now uses `Head[args]`, capitalized built-ins, and curly brace lists.
`to_full_form` prints square brackets to match; the Expr node names and all public
declarations in `core/include/symats/*.h` are unchanged. Header comment examples
were updated to describe the new notation. No backend interface changed.

## Takeover by Codex — T-032 task syntax audit

The delegated task plans below use Tier 1 public text examples. C++ method calls
such as `Session::run(expr, suppressed)` remain C++ API notation. No public API
or expression-spec change is part of this audit.

## TAKEOVER NOTICE (2026-10-03) — Claude is out of usage; tasks delegated
Claude's lane (core/, backend/) is temporarily owned by **Codex** (engine) and **Jules**
(tests, GMP, Jupyter kernel). Follow docs/COORDINATION.md takeover rules: work on
`takeover/<task>` branches, do not change public APIs in core/*.h or docs/EXPR_SPEC.md
without noting it here. Claude will review when back.

### Plans for the delegated tasks
- **T-010 native D and Expand (Codex)** — new `core/include/symats/calculus.h`, `core/src/calculus.cpp`.
  `D[f, x]`: rules for Plus (linearity), Times (product rule over all factors), Power
  (a^b: b*a^(b-1)*a' when b free of x; general case a^b*(b'*Log[a] + b*a'/a)),
  Sin/Cos/Tan/Exp/Log/ArcSin/ArcCos/ArcTan/Sinh/Cosh/Tanh/Abs (chain rule), numbers/other
  symbols -> 0, unknown `f[u]` -> `Derivative[1][f][u]*u'`. `D[f, {x, n}]`, `D[f, x, y]`.
  Evaluate `Derivative[n][f][t]` when f has a definition. `Expand`: distribute Times over
  Plus and expand integer powers of sums (multinomial), recursively. Register as built-ins
  in install_builtins (eval.cpp). Verify by numeric spot checks at random points.
- **T-018 Flat/Orderless matching (Codex)** — in pattern.cpp, for heads with Orderless
  attribute try argument permutations; for Flat heads let a pattern match a sub-sum
  (group remaining args under the head). Needs Context attributes passed to match().
- **T-012 Giac bridge (Codex, after T-011)** — `backend/giac/`: class GiacBackend :
  MathBackend; convert Expr <-> giac::gen by head name table (EXPR_SPEC), status
  Unverified; Integrate, Limit, Series, Solve, Factor, Simplify, DSolve, Det/Inverse/Eigen*.
- **T-013 verification (Codex)** — after a backend result: Integrate -> `D[result, x] - f == 0`
  via Expand/Together, else numeric check at 5 random points (status Verified/Unverified);
  Solve -> substitute; DSolve -> substitute equation and conditions.
- **T-009 GMP backend (Jules)** — keep symats::Integer API; CMake option SYMATS_USE_GMP;
  internals switch to mpz_class; all existing tests must pass both ways.
- **T-027 xeus kernel (Jules)** — see board.

## Review fixes (2026-10-09, from an independent review of this branch)
- NIntegrate declined nothing: divergent/oscillatory integrals (`NIntegrate[1/x, {x, 0, 1}]`,
  `NIntegrate[Sin[x]/x, {x, 1, Infinity}]`) came back as finite numbers. Now an estimate must have error <= 1e-6*Abs[value]
  (or 1e-10). Same for Integrate's NIntegrate fallback.
- `numeric::decimal` was off by 10^300 below ~1e-289. Fixed.
- Boost.Math threw on poles/overflow (killed `Plot[Gamma[x], {x, -3, 3}]`); now a quiet policy
  returns NaN/inf, and plot/verification catch evaluation errors.
- Native Series accepted non-finite numeric coefficients (`Abs[0]^-1`); now declines.
  `Abs[number]` evaluates.
- Simpson check: depth 18, one sample without parameters, polls abort (57 s -> 0.1 s).
- Native results only numerically confirmed now report Numeric, not Exact.
- `register_functions` called once (std::call_once).
- Definite native integrals: when f and F are both entire (polynomials, Exp, Sin, Cos, Sinh,
  Cosh, c^u), `F[b] - F[a]` is used without the quadrature check, so
  `Integrate[Sin[1000*x], {x, 0, 1000}]` is exact again.

## Current task (2026-10-09)
Claude is back. Branch `claude/great-heisenberg-mwf0uy`.

### T-013 numeric fallback — done (review)
- `core/include/symats/numeric.h` + `core/src/numeric.cpp`: the double evaluator now lives
  in core (elementary functions); `backend/common` only adds the Boost.Math table
  (`special_functions()`). New public header, no existing API changed.
- `verification_status(request, result)` -> Verified (exact) / Numeric (holds at 6 seeded
  sample points in [0.2, 2.2], >= 4 finite; definite integrals by adaptive Simpson) /
  Unverified. `verify_backend_result` unchanged (exact only). Giac now uses
  `verification_status`, so e.g. `Integrate[x^2, {x, 0, 1}]` reports Numeric.
- DSolve: solution rules `f[x] -> body` are applied as functions (also inside
  `Derivative[n][f][a]`, `f[0]`, mixed partials for PDEs), then every equation and initial
  condition is checked (exact, then numeric). Before this, DSolve results never verified.
- Tests: `tests/test_verification.cpp` (5 cases).

### T-038 abort — done (review)
- `Context::request_abort/clear_abort/abort_requested` (std::atomic<bool>), checked at every
  rewrite step in `evaluate`; throws `EvaluationAborted` (an `EvaluationError`).
- `Session::abort()`; `run_cell` clears the flag at start and stops after an aborted
  statement (`StatementResult::aborted`). Plot's point-by-point fallback rethrows aborts.
- For T-022 (Codex/Copilot): run cells on a worker thread, call `Session::abort()` from the
  UI thread. For cli/ (Copilot): a SIGINT handler may call it (lock-free atomic store).
  Backends: `backend_abort_requested()` (backend.h) is true inside backend calls of an
  aborted evaluation (thread-local `BackendAbortScope` set by the evaluator); odeint polls it
  every 64 steps, VEGAS every iteration. Nested 1-3 D quadrature does not (bounded cost).

### T-037 PDE NDSolve — done (review)
- `backend/odeint/pde.{h,cpp}`, reached from OdeintBackend when NDSolve has two ranges.
  Shared driver moved to `backend/odeint/integrator.h` (budgeted dopri5 -> Rosenbrock;
  finite-difference Jacobian template).
- Central differences, ghost nodes for Neumann, Dirichlet nodes driven by `a'[t]` (`a''[t]` for
  wave). Second-order accurate in the node spacing h (L/50) (tests: heat ~2e-3 relative, wave/Neumann < 5e-3).
- `numeric::GridSamples` / `interpolating_grid` in core + evaluator support for `sol[x, t]`.
- Not done: 2 space dimensions, systems of PDEs, adaptive/finer grids, Robin conditions.

### T-036 Integrate fallback chain — done (review)
- `core/include/symats/native_backend.h`: `NativeBackend` ("native", supports Integrate) and
  `native_antiderivative`. Forms: x^n / (a x + b)^n, 1/(a x + b), c^(a x + b), Exp, Sin, Cos,
  Tan, Cot, Sinh, Cosh, Tanh, Log, Sec[u]^2, Csc[u]^2, 1/(1 + x^2), 1/Sqrt[1 - x^2]; linearity;
  expands products/powers of sums. Accepted only if `D[F, x] - f` is exactly 0 (after Expand and
  Tan/Sec/... -> Sin/Cos). Definite: `F[b] - F[a]` if the quadrature check agrees, else
  `NIntegrate[f, range]` with status Numeric (numeric integrand only).
- install_default_backends adds it last. Also `Exp[1] -> E`.
- Also `Series[f, {x, a, n}]` (n <= 12) as a Taylor polynomial in `SeriesData`, declining
  when a coefficient is singular (ComplexInfinity, Indeterminate, Log[0]).
- Also `Solve`: one polynomial equation of degree 1-2 (coefficients via derivatives at 0;
  quadratic formula, complex roots stay as radicals), linear systems (LinearSolve for
  numeric coefficients, Cramer for <= 3 symbolic unknowns). Kept only if substitution
  verifies (verification_status != Unverified).
- Also `Limit[f, x -> a]` (finite numeric a): substitution or L'Hopital on 0/0, kept only if
  f is numerically within 1e-3 of it on both sides.
- Integration by parts for polynomial times Exp/Sin/Cos/Sinh/Cosh/c^u of a linear argument.
- Also `DSolve[eqns, y[x], x]`: linear, constant numeric coefficients; order 1 with any
  forcing the native integrator handles (integrating factor), order 2 with constant forcing
  (real / double / complex characteristic roots); fresh constants C1, C2; conditions
  `y[x0] == v`, `y'[x0] == v` solved as a linear system. Kept only if the DSolve verifier
  passes.
- Also `Factor[p]`: one variable, rational coefficients, degree <= 12; content, rational-root
  linear factors with multiplicity (|constant|, |leading| <= 10^7 for the divisor search), rest
  kept; only if Expand proves it.
- Notation audit (2026-10-09): comments/docs on this branch and AGENTS.md plot examples now use
  Tier 1 (`==` for equations, `{x, y}` points, no `[[ ]]`). Two old commit messages still
  read `{t, a, b(, dt)}` and `0*infinity`; fixing them needs a force-push (not permitted in
  this session). Local work branch: `claude-work`, pushed to the designated branch.
- Fixed canonical arithmetic: `0*ComplexInfinity` and `0*Infinity` give `Indeterminate` (was 0, so
  `Sin[x]/x /. x -> 0` gave 0), `Infinity - Infinity` gives `Indeterminate`, infinities absorb
  finite terms, Indeterminate absorbs everything.
- Proposal (not done, needs Codex for parser/printer): an `Expr` Real kind (double first,
  MPFR later) so numeric results print as decimals and `N[...]` can exist.

### T-035 native linear algebra — done (review)
- `core/src/linalg.{h,cpp}` (private header), installed from install_builtins. Rational
  Gauss-Jordan; CharPoly by Faddeev-LeVerrier. Built-ins run before backends, so only
  exact-number matrices are taken (Transpose/Trace any entries); symbolic -> Giac.
- Tests: `tests/test_linalg.cpp` (3 cases). Full Giac+SUNDIALS+Boost ctest still 7/7.

### T-010 review (Codex's native D/Expand) — reviewed, fixed in place
- Behaviour checked on 23 edge cases (chain/product/power rules, x^x, Log[b, x],
  higher/mixed partials, Expand of powers and products): correct.
- Fixed: `D[E^x, x]` gave `E^x*Log[E]`; ArcSinh/ArcCosh/ArcTanh had no derivative rules.
  Added exact values Sin/Tan/.../Exp at 0, `Log[1] == 0`, `Log[E] == 1` (eval.cpp) and made the
  inverse hyperbolic functions Listable. Tests: `tests/test_elementary.cpp`.

### T-034 Plot sampling — done (review)
- `core/include/symats/plot.h`, `core/src/plot.cpp`: `sample_curve` (64 intervals, up to 10
  bisections, tolerance 1e-3 of the robust y range) and the `Plot` built-in (HoldAll; the
  variable is substituted by a fresh symbol before evaluating, like Block; falls back to
  point-by-point evaluation when the body does not compile).
- Output shape documented in EXPR_SPEC §3.9 — Codex: this is what app/ Plotly should render.
- `numeric.h`: `Samples` (cubic interpolation), `register_functions` / `default_functions`;
  install_default_backends registers the Boost.Math table so `Plot[Gamma[x], {x, 1, 3}]` works.
- Also ParametricPlot, PolarPlot (shared 2-D adaptive sampler), Plot3D / ContourPlot
  (51x51 grid -> SurfaceGrid / ContourGrid for Plotly), ImplicitPlot and
  ContourPlot[eqn] (marching squares, segments joined via shared grid edges).
- Animate / Slider: `Animation[{frames}, {t, {values}}(, Control -> Slider)]`, 31 frames by
  default, all frames rewritten to the union PlotRange. Codex: frame format for Plotly is
  in EXPR_SPEC §3.9; propose changes there if the app needs something else.

### T-033 default backends
- `backend/registry`: `install_default_backends(registry)` adds whatever was built: Giac,
  SUNDIALS, Odeint, quadrature (that order). Called by symats-cli and the kernel.
- Follow-up idea for Codex (convert/): print `InterpolatingFunction[{a, b}, <>]` instead of
  101 sample pairs, like Mathematica.

### T-026 NIntegrate — done (review)
- `backend/quadrature/` (`QuadratureBackend`), built with the odeint backend under the
  renamed option `SYMATS_USE_BOOST` (was SYMATS_USE_ODEINT).
- Shared numeric compiler moved to `backend/common/numeric_expr.{h,cpp}` (slots, Boost.Math
  special functions, `decimal()`), used by odeint and quadrature.
- 1-3 dims: nested tanh-sinh / exp-sinh / sinh-sinh, Gauss-Kronrod 61 fallback; inner
  limits may depend on outer variables; +-Infinity limits. >= 4 dims: VEGAS (own
  implementation in `vegas.{h,cpp}`, Lepage 1978; not ported from Numerica, so no
  attribution needed), fixed seed, finite limits only.
- `last_error()` / `last_method()` on the backend. Error-tracking floats need a Real kind
  in Expr (EXPR_SPEC "later kinds") — not done.

### T-015 follow-up — SUNDIALS adapter fixed
The merged adapter never solved anything and did not build on Ubuntu (SUNDIALS 6.4):
`SUN_COMM_NULL`/`realtype` are 7.x-only (now version-guarded / `sunrealtype`), the test
lacked test_main.cpp + tests/ include, `y'[x]` was matched as `Derivative[1][y[x]]`
instead of `Derivative[1][y][x]`, `y[x]` in the RHS was never recognised, and the
first sample asked CVODE for tout == t0. All fixed; tests now check `y[1] == E^-1`.

### T-029 Boost.Odeint NDSolve — done (review)
- `backend/odeint/` (`OdeintBackend`, CMake option `SYMATS_USE_BOOST`, header-only Boost >= 1.71).
- Any order, any number of unknowns: reduced to a first-order system. Equations must be
  explicit in the highest derivative (either side of `==`); all initial values at t0.
- RHS compiled once to a numeric node tree: elementary functions + Boost.Math
  (Gamma, Gamma[a,z], LogGamma, Beta, Erf, Erfc, Zeta, BesselJ/Y/I/K).
- Dormand-Prince 5(4) dense output; if it exceeds 20000 steps -> Rosenbrock 4 with a
  symbolic Jacobian from native `differentiate` (finite differences if it does not compile).
- Result: single function -> bare `InterpolatingFunction[{t0, t1}, {{t, y}, ...}]` (same
  shape as SUNDIALS); list -> `{x -> IF, y -> IF}`. 101 samples, 12-digit decimal rationals.
- Note: Prothero-Robinson `y'[t] == -10^6 (y[t] - Cos[t])` makes Rosenbrock4 stall (order
  reduction); it declines after the 200000-step budget. Robertson and Van der Pol work.
- Tests: `backend/odeint/test_odeint_backend.cpp` (7 cases), clean under ASan/UBSan.
- CI: Linux job in ci.yml installs libboost-dev and enables the backend. Windows not yet
  (needs vcpkg boost-odeint + boost-math).

## T-021 — what was built
- `core/include/symats/session.h`, `core/src/session.cpp`: `Session` (one per notebook):
  `run(expr, suppressed)` -> `StatementResult{line, input, output, status, suppressed,
  error}` with `visible()` (hides suppressed, Null, errors); `run_cell(statements)`;
  `out(n)`, `history()`, `next_line()`; `restart()` keeps registered backends;
  `user_symbols()` for the workspace panel. Built-in `Out[]`, `Out[n]`, `Out[-k]`.
- `CompoundExpression` built-in (HoldAll) in core/eval.cpp; `Context::user_symbols()`.
- Tests: `tests/test_session.cpp` (6 cases). 58/58 total pass under ASan/UBSan.
- Spec: AGENTS.md "Notebook model" (bare UI rules, cells, Shift+Enter, `;`, history,
  workspace, `.sym` scripts); EXPR_SPEC rows for CompoundExpression and Out.

## T-001 — what was built
- `core/include/symats/pattern.h`, `core/src/pattern.cpp`: `match` (Blank, `Blank[h]`,
  BlankSequence, BlankNullSequence, named patterns, repeated names, backtracking),
  `substitute` (splices Sequence), `replace_all` (one top-down pass), helpers
  `blank/pat/pat_seq/pat_null_seq`, `has_pattern`, `head_name`.
- `core/include/symats/backend.h`, `core/src/backend.cpp`: `MathBackend` interface,
  `BackendResult{value, status, backend}`, `ResultStatus` (exact/verified/numeric/
  unverified), `BackendRegistry` (ordered; a throwing backend counts as declining).
- `core/include/symats/eval.h`, `core/src/eval.cpp`: `Context` (own values, down values
  with exact-before-pattern ordering and replacement on equal lhs, attributes, built-ins,
  backend registry, depth/iteration limits), `evaluate`, `evaluate_top` (returns status).
  Built-ins: Plus, Times, Power, Set, SetDelayed, Clear, Substitute/ReplaceAll, Equal,
  Unequal, Less(Equal), Greater(Equal). Attributes: Hold*, Listable, Protected, etc.
- Tests: `tests/test_pattern.cpp` (7 cases), `tests/test_eval.cpp` (10 cases).

## Verification done
- 47/47 test cases pass with -Wall -Wextra -Wpedantic -Werror, ASan + UBSan, and with
  `<cmath>` force-included (MSVC-like).
- 50,000 random raw expressions: evaluate() equals the canonical builders (0 diffs) and
  is idempotent (0). Lists excluded from the comparison because Listable threading is
  intentionally different from the raw builders.
- 1 MB stack (Windows default), -O0: recursion limit (max_depth 400) throws cleanly;
  fact(150) works.

## Decisions / notes for others
- Iteration budget resets at every outermost `evaluate` call (not per session).
- A definition that rewrites an expression to itself is treated as not applying.
- `x = x + 1` throws "recursion depth limit exceeded" (same as other CAS).
- Known limitation: matching is structural; `a_ + b_` does not yet match a 3-term sum
  (Flat/Orderless matching → new task T-018).
- Text parser does not yet produce Pattern nodes from `x_` (Codex → new task T-017).

## Open questions
None.
