# Task board

Statuses: `todo` → `in-progress` → `review` (PR open) → `done`; also `blocked`.
`Blocks` = tasks that cannot start until this one is done (takeover priority).
`backup-ok` = a backup may start this task while the owner is out.
`Updated` must be refreshed at every checkpoint (UTC). Stale after 12 h.

All tasks that accept, display, print, document, or test public text use the Tier 1
syntax in `AGENTS.md` and `docs/EXPR_SPEC.md`: capitalized, case-sensitive built-ins,
square-bracket calls (`Sin[x]`), curly-brace lists (`{a, b}`), and parentheses only
for grouping. User-defined names may be lowercase (`f[x]`). Internal C++ API names
and MathJSON keys are unaffected. Tier 2 and Tier 3 operators are outside the
scope of every task on this board.

| ID | Task | Lane | Owner | Backup | Status | Blocks | Flags | Updated (UTC) |
|----|------|------|-------|--------|--------|--------|-------|---------------|
| T-008 | Install Git + C++ workload; build in VS 2022; push repo to GitHub — built in VS 2026, pushed, CI green | setup | Loc | — | done | T-004, T-005, all cloud work | | 2026-10-02 |
| T-003 | Decide: `=` means assignment (Set) or equation (Equal)? **Decided: `=` → `Set`, `==` → `Equal`, `:=` → `SetDelayed`** (spec updated) | spec | Loc | — | done | | | 2026-10-02 |
| T-031 | Canonical Tier 1 text syntax: square bracket calls, capitalized built-ins, curly brace lists/matrices; align printer, tests, docs and future tasks — merged (PR #15) | convert/spec | Codex | Claude | done | T-019, T-020, T-024 | | 2026-10-05 17:12 |
| T-032 | Audit every task for canonical Tier 1 text syntax; clarify the board-wide rule and correct stale Codex handoff examples — PR #16 | coordination | Codex | Claude | done | | | 2026-10-08 |
| T-001 | Evaluator: definitions (`Set`, `SetDelayed`), pattern matching (`x_`, `x_Integer`), rule replacement, attributes; **plus `MathBackend` interface in core/ and dispatch of math heads (`Integrate`, `Solve`, `DSolve`, …) to the registered backend** — merged (PR #5) (pattern.h, eval.h, backend.h; 17 new test cases) | core | Claude | Codex | done | T-010, T-012 | | 2026-10-02 |
| T-002 | Converter fixes: (1) nesting-depth limit instead of crash, (2) readable output (`x - y`, `-x^2`, `Sin[x]`, `==`, `->`), (3) decimals, (4) document/decide space-before-paren, (5) `=` per T-003 | convert | Codex | Claude | done | | backup-ok | 2026-10-02 03:38 |
| T-004 | Fix CI build failures on Windows and Linux — merged (PR #10), restoring test function linkage | build/CI | Codex (for Copilot) | Codex | done | | backup-ok | 2026-10-05 17:12 |
| T-005 | Tests: parser edge cases (unicode, whitespace, huge numbers, malformed input), Integer/Rational property tests — PR #1 merged | tests | Jules | Copilot | done | | backup-ok | 2026-10-02 |
| T-006 | `symats-cli` REPL: read Tier 1 square bracket input → `parse_text` → print `to_text` — completed with interactive REPL and Session execution | cli | Jules (for Copilot) | Codex | done | | backup-ok | 2026-10-08 |
| T-007 | MathJSON ⇄ Expr converter — merged (PR #3). Follow-up: allow Unicode symbols (α, θ) | convert | Codex | Claude | done | app editor | | 2026-10-02 |
| T-009 | Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP` — merged (PR #12) | core | Jules (for Claude) | Copilot | done | | backup-ok | 2026-10-05 |
| T-010 | **Native** `D` (partial derivatives, chain/product rule, all elementary functions) and `Expand` in core/ — no Giac; these power result verification — PR #21 | core | Codex (for Claude) | Codex | done | T-013 | | 2026-10-05 18:08 |
| T-011 | Giac build: fork Giac under nxuanloc2026, pin a release, add as git submodule in `third_party/giac` (+ GMP via vcpkg), CMake target `third_party::giac`, building on Windows (MSVC) and Linux CI — PR #22 | third_party/build | Codex (for Copilot) | Codex | done | T-012 | | 2026-10-06 13:28 |
| T-012 | **Giac bridge**: `backend/giac/` implements `MathBackend`; `Expr ⇄ giac::gen` conversion for all EXPR_SPEC heads; first operations: Integrate, Limit, Series, Solve, Factor, Simplify, DSolve, linear algebra — merged (PR #23), compile fix validated (PR #24) | backend | Copilot | Codex | done | T-013, T-014 | | 2026-10-08 23:45 |
| T-013 | Verification layer: check integrals (differentiate), solutions (substitute), ODE solutions (substitute + conditions), matrix results; numeric fallback check with Arb/Boost; result status `verified`/`numeric`/`unverified` — exact verification implemented, numeric fallback remains | core | Copilot (backup for Claude) | Codex | done | app result display | numeric fallback deferred until numeric backend/representation exists | 2026-10-08 23:56 |
| T-014 | Backend comparison suites: Rubi integral test set (MIT) and SymPy cases (BSD) run through Symats in CI; report mismatches — merged (PR #4) | tests | Jules | Copilot | done | | backup-ok | 2026-10-02 |
| T-015 | SUNDIALS: add to `third_party/` (Copilot builds) and `backend/sundials/` NDSolve adapter (Claude) | backend | Copilot (for Claude) | — | review | numeric plots of ODEs | takeover: Claude unavailable; PR #30 | 2026-10-08 23:58 |
| T-016 | App shell — **superseded by T-019** (bare notebook UI per AGENTS.md "Notebook model") | app | Codex | Claude | blocked | | | 2026-10-02 |
| T-017 | **Merged into T-024** (pattern syntax is part of Tier 1 parsing) | convert | Codex | Claude | done | | backup-ok | 2026-10-02 |
| T-018 | Flat/Orderless-aware matching so `a_ + b_` matches sums with any number of terms (core/pattern.cpp) — check behavior against **Mathics3** (GPL, reference only) | core | Codex (for Claude) | Codex | todo | | | 2026-10-03 |
| T-019 | **Bare notebook UI** per AGENTS.md "Notebook model": cells with In[n]/Out[n] gutter, Shift+Enter, multi-statement cells, inline errors, minimal top bar (run/abort/restart/workspace), hover-only controls, MathLive per-cell toggle, KaTeX output, and canonical capitalized square bracket text. Replace the current card layout — **build on CodeMirror 6** (input cells), MathLive (2-D), KaTeX + STIX Two (output), Plotly (plots) | app | Codex | Claude | todo | | | 2026-10-02 |
| T-020 | `parse_cell(text)` in convert/: split a cell into statements (newline / `;`, continuation inside square bracket calls, curly brace lists, or after an operator), mark trailing-`;` suppression; parse `%`, `%%`, `%n` → `Out[-1]`, `Out[-2]`, `Out[n]`; `a; b` → CompoundExpression | convert | Jules | Claude | done | T-019 | backup-ok | 2026-10-08 |
| T-021 | Kernel: `core/session.h` — shared Context across cells, In/Out history, per-statement results with status/errors, Out builtin, CompoundExpression, restart, `user_symbols()` for the workspace panel — done on branch claude/T-021-session (+ Tier 1 engine operators) | core | Claude | Codex | done | T-019, T-022 | | 2026-10-02 07:30 |
| T-022 | Engine bridge: run the C++ Session behind the app (Tauri command or WebAssembly), async evaluation with abort; JSON messages {cell statements} → {results: MathJSON + LaTeX + status + error} | bridge | Codex (for Copilot) | Claude | todo | T-019 | | 2026-10-02 |
| T-023 | Scripts: canonical Tier 1 square bracket `.sym` import/export and `symats-cli file.sym` runner (extends T-006) — completed in `cli/main.cpp` | cli | Jules (for Copilot) | Claude | done | | backup-ok | 2026-10-08 |
| T-024 | Parser: remaining **Tier 1 operators** per EXPR_SPEC §3.12 — `/.`, `.` (Dot vs decimal), `y'[t]` → `Derivative[1][y][t]`, `&&`, `||`, prefix `!` vs postfix `!`, plus precedence and printer round-trips; printer emits the same operators — completed | convert | Jules | Claude | done | T-019 | backup-ok | 2026-10-08 |
| T-026 | Port from Numerica (MIT, with attribution): Vegas Monte Carlo integration for NIntegrate of multiple integrals; error-tracking floats for numeric result status | core/numeric | Codex (for Claude) | Codex | todo | | | 2026-10-03 |
| T-027 | Jupyter kernel with **xeus** (BSD-3): new folder `kernel/`, wraps `symats::Session` + Tier 1 square bracket `parse_cell` (T-020) so Symats runs in JupyterLab / VS Code notebooks; returns text/plain + text/latex — completed | kernel | Jules | Codex | done | | | 2026-10-08 |
| T-028 | Tests: Session (multi-cell, %, errors), Tier 1 operators, and D/Expand expected values from mathematics, SymPy, Maxima — completed | tests | Jules | Codex | done | | backup-ok | 2026-10-08 |
| T-029 | Quick NDSolve with **Boost.Odeint** (RK45/Dormand–Prince, Rosenbrock for stiff) in `backend/odeint/` + numeric special functions via **Boost.Math**; SUNDIALS (T-015) later | backend | Codex (for Claude) | Jules | todo | numeric ODE plots | backup-ok | 2026-10-03 |
| T-030 | Evaluator semantics audit vs Mathics3 — completed | tests | Jules | Codex | done | | backup-ok | 2026-10-08 |
