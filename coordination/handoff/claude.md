# Handoff — Claude

## Current task
T-021 (kernel Session) done on branch `claude/T-021-session` (review — needs push + PR).
T-001 merged (PR #5). Next: T-010 (native `D` and `Expand`), then T-012 (Giac bridge).

## T-021 — what was built
- `core/include/symats/session.h`, `core/src/session.cpp`: `Session` (one per notebook):
  `run(expr, suppressed)` -> `StatementResult{line, input, output, status, suppressed,
  error}` with `visible()` (hides suppressed, Null, errors); `run_cell(statements)`;
  `out(n)`, `history()`, `next_line()`; `restart()` keeps registered backends;
  `user_symbols()` for the workspace panel. Built-in `Out()`, `Out(n)`, `Out(-k)`.
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
