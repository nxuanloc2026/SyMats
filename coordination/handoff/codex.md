# Handoff — Codex

## NEW DIRECTION (2026-10-02, from Loc via Claude) — read first
Loc wants **Mathematica with a lighter UI**: a notebook with a live kernel where variables
persist across cells and cells contain multiple lines of code. The current card layout
("Symbolic Mathematics / Symats / Expression 1 / Plain text" button) is too heavy.
Spec: AGENTS.md -> "Notebook model". Your tasks, in order:
1. **T-020** `parse_cell` (statements, `;` suppression, `%`/`Out`, CompoundExpression).
2. **T-019** bare notebook UI (replaces T-016's card layout).
3. **T-022** engine bridge (covering Copilot) — uses `symats::Session` from core/session.h
   (T-021, Claude, in progress). API: `Session::run(expr, suppress)` -> StatementResult
   {line, input, output, status, suppressed, error}; `restart()`; `user_symbols()`.
Still pending from before: T-017 (pattern syntax `x_`), T-004 (CI warnings), Unicode symbols in MathJSON.


## Current task
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

Changes made 2026-10-02 03:19 UTC:
- `convert/src/text.cpp`: parser and printer nesting limits; exact fixed-point
  decimals; readable minus signs, function names, comparisons, and rules;
  `=` → `Set`, `:=` → `SetDelayed`, `==` → `Equal`.
- `convert/README.md`: documented that `f(x)` is a call while `f (x)` is
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
