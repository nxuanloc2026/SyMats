# Handoff — Codex

## Current task
T-016a editor shell claimed 2026-10-02 04:59 UTC. This is the first small
slice of T-016: a MathLive editor cell, plain-text editor, and mode toggle
through an injected converter interface. The C++ `bridge/` is not present yet,
so the app must report when conversion is unavailable rather than bypass Expr.

MathJSON Unicode note: the MathJSON specification requires non-ASCII symbols
in an explicit `{ "sym": "α" }` object or backtick shorthand. The converter
already supports both. A bare `"α"` is a string, not a symbol, so no converter
fix is needed.

Git note: this managed checkout started at T-007 commit `699a6fe`; PR #3 is
merged. Git metadata under the main checkout is protected from this sandbox,
so normal branch creation/fetch is denied. A separate Git directory at
`%TEMP%/symats-t016a-codex.git` has a `codex/T-016a-editor-shell` branch based
on current `main`; its worktree is this Codex checkout. The branch is committed
locally, but a push failed and it is not yet on GitHub.

Checkpoint 2026-10-02 05:11 UTC:
- Added `app/` Vite shell with MathLive input, Compute Engine MathJSON adapter,
  text editor pane, and toggle controller using an injected Symats converter.
- The Compute Engine loads when conversion is first needed. No local fallback
  bypasses the shared Expr tree; without `bridge/`, the toggle is disabled.
- `pnpm test`: 4 tests pass. `pnpm build`: succeeds. Browser smoke check showed
  the math field and bridge status message. `mathlive/fonts.css` fixed the font
  loading error seen in the first browser run. Vite warns about the Compute
  Engine bundle size; it is split from the initial editor bundle.
- Next: push the local branch and open a PR when GitHub authentication works.
  Then connect `globalThis.symatsConverter` when Copilot's `bridge/` is ready.
  An unauthenticated HTTPS push failed with `could not read Username for
  https://github.com`; the remote branch does not exist yet. The local commit
  is on `codex/T-016a-editor-shell` in the separate Git directory above.
  A verified recovery bundle is at `work/T-016a-editor-shell.bundle` in this
  worktree, so the commit survives even if the temporary Git directory is lost.

## Previous task — T-007
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
