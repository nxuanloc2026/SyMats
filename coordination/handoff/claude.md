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

## Current task (2026-10-09)
Claude is back. Branch `claude/great-heisenberg-mwf0uy`.

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
first sample asked CVODE for tout == t0. All fixed; tests now check y(1) = e^-1.

### T-029 Boost.Odeint NDSolve — done (review)
- `backend/odeint/` (`OdeintBackend`, CMake option `SYMATS_USE_ODEINT`, header-only Boost >= 1.71).
- Any order, any number of unknowns: reduced to a first-order system. Equations must be
  explicit in the highest derivative (either side of `==`); all initial values at t0.
- RHS compiled once to a numeric node tree: elementary functions + Boost.Math
  (Gamma, Gamma[a,z], LogGamma, Beta, Erf, Erfc, Zeta, BesselJ/Y/I/K).
- Dormand-Prince 5(4) dense output; if it exceeds 20000 steps -> Rosenbrock 4 with a
  symbolic Jacobian from native `differentiate` (finite differences if it does not compile).
- Result: single function -> bare `InterpolatingFunction[{t0,t1}, {{t,y},...}]` (same
  shape as SUNDIALS); list -> `{x -> IF, y -> IF}`. 101 samples, 12-digit decimal rationals.
- Note: Prothero-Robinson `y' == -10^6 (y - Cos[t])` makes Rosenbrock4 stall (order
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
