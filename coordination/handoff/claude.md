# Handoff — Claude

## Takeover by Codex — T-031 text syntax

Loc requested the Tier 1 square bracket notation across Symats. The expression
specification now uses `Head[args]`, capitalized built-ins, and curly brace lists.
`to_full_form` prints square brackets to match; the Expr node names and all public
declarations in `core/include/symats/*.h` are unchanged. Header comment examples
were updated to describe the new notation. No backend interface changed.

## TAKEOVER NOTICE (2026-10-03) — Claude is out of usage; tasks delegated
Claude's lane (core/, backend/) is temporarily owned by **Codex** (engine) and **Jules**
(tests, GMP, Jupyter kernel). Follow docs/COORDINATION.md takeover rules: work on
`takeover/<task>` branches, do not change public APIs in core/*.h or docs/EXPR_SPEC.md
without noting it here. Claude will review when back.

### Plans for the delegated tasks
- **T-010 native D and Expand (Codex)** — new `core/include/symats/calculus.h`, `core/src/calculus.cpp`.
  `D[f, x]`: rules for Plus (linearity), Times (product rule over all factors), Power
  (a^b: b*a^(b-1)*a' when b free of x; general case a^b*(b'*Log(a) + b*a'/a)),
  Sin/Cos/Tan/Exp/Log/ArcSin/ArcCos/ArcTan/Sinh/Cosh/Tanh/Abs (chain rule), numbers/other
  symbols -> 0, unknown `f[u]` -> `Derivative[1][f][u]*u'`. `D[f, {x, n}]`, `D[f, x, y]`.
  Evaluate `Derivative[n, f](t)` when f has a definition. `Expand`: distribute Times over
  Plus and expand integer powers of sums (multinomial), recursively. Register as built-ins
  in install_builtins (eval.cpp). Verify by numeric spot checks at random points.
- **T-018 Flat/Orderless matching (Codex)** — in pattern.cpp, for heads with Orderless
  attribute try argument permutations; for Flat heads let a pattern match a sub-sum
  (group remaining args under the head). Needs Context attributes passed to match().
- **T-012 Giac bridge (Codex, after T-011)** — `backend/giac/`: class GiacBackend :
  MathBackend; convert Expr <-> giac::gen by head name table (EXPR_SPEC), status
  Unverified; Integrate, Limit, Series, Solve, Factor, Simplify, DSolve, Det/Inverse/Eigen*.
- **T-013 verification (Codex)** — after a backend result: Integrate -> D(result) - f == 0
  via Expand/Together, else numeric check at 5 random points (status Verified/Unverified);
  Solve -> substitute; DSolve -> substitute equation and conditions.
- **T-009 GMP backend (Jules)** — keep symats::Integer API; CMake option SYMATS_USE_GMP;
  internals switch to mpz_class; all existing tests must pass both ways.
- **T-027 xeus kernel (Jules)** — see board.

## Current task
T-021 (kernel Session) done on branch `claude/T-021-session` (review — needs push + PR).
T-001 merged (PR #5). Next: T-010 (native `D` and `Expand`), then T-012 (Giac bridge).

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
- `core/include/symats/pattern.h`, `core/src/pattern.cpp`: `match` (Blank, Blank(h),
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
