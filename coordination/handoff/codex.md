# Handoff — Codex

## T-031 Tier 1 text syntax (2026-10-05)

Branch: `codex/T-031-square-bracket-syntax`, based on main `d85f00c`.
The user-facing text grammar uses `Sin[x]` and `f[x_] := Sin[x]`,
capitalized built-ins, and curly brace lists/matrices. The internal Expr node
names stay the same. Converter, full-form printer, regression tests, specifications,
and queued task descriptions are updated. Windows Debug and Release builds and
the full test suite pass. Next: check GitHub CI, then mark the PR ready for review.

## T-004 CI repair (2026-10-05 UTC)

Branch: `codex/T-004-ci-build-fix`, based on main `842de0e`.
PR: https://github.com/nxuanloc2026/Symats/pull/10 (human merge required).
Restored internal linkage in `TEST_CASE`: generated names use `__LINE__`, so
removing `static` caused duplicate symbols between test translation units and
broke linking on Windows and Linux. Existing same-line tests exercise this
regression; no new API or expression-spec changes are needed.

Windows Debug and Release builds and all 71 test cases pass. PR CI
is being checked before review. The original checkout has unresolved local
coordination-file conflicts; this fix uses the `ci-build-fix` managed worktree.

## NEW DIRECTION (2026-10-02, from Loc via Claude) — read first
Loc wants **Mathematica with a lighter UI**: a notebook with a live kernel where variables
persist across cells and cells contain multiple lines of code. The current card layout
("Symbolic Mathematics / Symats / Expression 1 / Plain text" button) is too heavy.
Spec: AGENTS.md -> "Notebook model". Your tasks, in order:
0. **T-024** Tier 1 operators in the parser (replaces T-017; see EXPR_SPEC §3.12 precedence table). Engine side already done: And/Or/Not, Dot, ReplaceAll.
1. **T-020** `parse_cell` (statements, `;` suppression, `%`/`Out`, CompoundExpression).
2. **T-019** bare notebook UI (replaces T-016's card layout).
3. **T-022** engine bridge (covering Copilot) — uses `symats::Session` from core/session.h
   (T-021, Claude, in progress). API: `Session::run(expr, suppress)` -> StatementResult
   {line, input, output, status, suppressed, error}; `restart()`; `user_symbols()`.
**Time-savers (approved in AGENTS.md):** T-019 must be built on CodeMirror 6 + MathLive + KaTeX/STIX Two + Plotly (assemble, don't write an editor). After T-010 add T-029 (Boost.Odeint NDSolve). Use Mathics3 (GPL) only as a behavior reference for T-018.
Still pending from before: T-004 (CI warnings), Unicode symbols in MathJSON.


## Current task
<<<<<<< Updated upstream
<<<<<<< Updated upstream
T-007 MathJSON ⇄ Expr converter claimed 2026-10-02 03:58 UTC in the
`mathjson-converter` worktree. Implementing JSON interchange in `convert/`;
the managed checkout is detached because Git metadata lock creation was denied.
The `codex/T-007-mathjson-converter` branch ref exists at the starting commit.
Implementation completed 2026-10-02 04:04 UTC:
- `convert/include/symats/mathjson.h`, `convert/src/mathjson.cpp`: parse and
  serialize MathJSON, exact numbers, canonical arithmetic, matrix/list,
  MathLive's structural and canonical integrals, logarithm name/argument
  mapping, limit, generic heads and non-symbol `Apply`.
- `tests/test_mathjson.cpp`: numeric, arithmetic, calculus, matrix, symbol,
  malformed input and round-trip coverage; test CMake list updated.
- `convert/README.md` documents supported forms and limits.

Validation: Visual Studio 2026 CMake configure/build and CTest passed;
28 test cases, zero failures. `git diff --check` found no whitespace errors.
The Git checkout is detached because its metadata locks were denied. A scoped
write permission grant for those exact paths still left
`.git/worktrees/Symats/index.lock` inaccessible to `git switch`. Branch ref
`codex/T-007-mathjson-converter` still points at the initial commit.
Next: attach checkout to that branch, commit these files, push branch and open
PR. Keep T-007 `in-progress` until the PR exists.
=======
None in progress. T-002 is complete; next: T-007 MathJSON ⇄ Expr converter.
>>>>>>> Stashed changes
=======
None in progress. T-002 is complete; next: T-007 MathJSON ⇄ Expr converter.
>>>>>>> Stashed changes

Changes made 2026-10-02 03:19 UTC:
- `convert/src/text.cpp`: parser and printer nesting limits; exact fixed-point
  decimals; readable minus signs, function names, comparisons, and rules;
  `=` → `Set`, `:=` → `SetDelayed`, `==` → `Equal`.
- `convert/README.md`: documented that `f[x]` is a call while `f (x)` is
  multiplication, plus decimal and assignment behavior.
- `tests/test_text.cpp`: focused examples and round-trip/deep-nesting checks.
- `coordination/BOARD.md` and `coordination/STATUS.md`: task state.

Validation completed 2026-10-02 03:37 UTC in Visual Studio 2026 Developer
PowerShell: `cmake --preset windows-debug`, `cmake --build --preset
windows-debug`, and `ctest --preset windows-debug` all passed (1/1 CTest
suite). The T-002 implementation is present in commit `ad2bb2b` on
`origin/main`, committed by the maintainer while Codex was troubleshooting
the local toolchain. The old `codex/T-002-converter-fixes` branch ref still
points to the initial commit and is not the implementation branch.

## Done
- `convert/`: `parse_text` / `to_text` (arithmetic, calls, lists, matrices, comparisons, rules),
  round-trip tests in `tests/test_text.cpp`. Verified by Claude: 200,000 random round trips pass.

## Known issues from Claude's review
All five listed T-002 issues were addressed and the current test suite passes.

## Open questions
_(Codex: keep this file updated at every checkpoint.)_
