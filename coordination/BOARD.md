# Task board

Statuses: `todo` → `in-progress` → `review` (PR open) → `done`; also `blocked`.
`Blocks` = tasks that cannot start until this one is done (takeover priority).
`backup-ok` = a backup may start this task while the owner is out.
`Updated` must be refreshed at every checkpoint (UTC). Stale after 12 h.

| ID | Task | Lane | Owner | Backup | Status | Blocks | Flags | Updated (UTC) |
|----|------|------|-------|--------|--------|--------|-------|---------------|
| T-008 | Install Git + C++ workload; build in VS 2022; push repo to GitHub — built in VS 2026, pushed, CI green | setup | Loc | — | done | T-004, T-005, all cloud work | | 2026-10-02 |
| T-003 | Decide: `=` means assignment (Set) or equation (Equal)? **Decided: `=` → `Set`, `==` → `Equal`, `:=` → `SetDelayed`** (spec updated) | spec | Loc | — | done | | | 2026-10-02 |
| T-001 | Evaluator: definitions (`Set`, `SetDelayed`), pattern matching (`x_`, `x__`, `x_Integer`), rule replacement, attributes; **plus `MathBackend` interface in core/ and dispatch of math heads (`Integrate`, `Solve`, `DSolve`, …) to the registered backend** — merged (PR #5) | core | Claude | Codex | done | T-010, T-012 | | 2026-10-02 |
| T-002 | Converter fixes: (1) nesting-depth limit instead of crash, (2) readable output (`x - y`, `-x^2`, `sin(x)`, `==`, `->`), (3) decimals, (4) document/decide space-before-paren, (5) `=` per T-003 | convert | Codex | Claude | done | | backup-ok | 2026-10-02 |
| T-004 | Fix CI build failures on Windows and Linux — PR #10 restores test function linkage | build/CI | Jules | Codex | review | | backup-ok | 2026-10-05 |
| T-005 | Tests: parser edge cases (unicode, whitespace, huge numbers, malformed input), Integer/Rational property tests — PR #1 merged | tests | Jules | Copilot | done | | backup-ok | 2026-10-02 |
| T-006 | `symats-cli` REPL: read a line → `parse_text` → print `to_text` | cli | Jules | Codex | review | | backup-ok | 2026-10-05 |
| T-007 | MathJSON ⇄ Expr converter — merged (PR #3). Follow-up: allow Unicode symbols (α, θ) | convert | Codex | Claude | done | app editor | | 2026-10-02 |
| T-009 | Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP` | core | Jules | Copilot | review | | backup-ok | 2026-10-05 |
| T-010 | **Native** `D` (partial derivatives, chain/product rule, all elementary functions) and `Expand` in core/ — no Giac; these power result verification | core | Jules | Codex | review | T-013 | | 2026-10-05 |
| T-011 | Giac build: fork Giac under nxuanloc2026, pin a release, add as git submodule in `third_party/giac` (+ GMP via vcpkg), CMake target `third_party::giac`, building on Windows (MSVC) and Linux CI | third_party/build | Jules | Codex | todo | T-012 | | 2026-10-05 |
| T-012 | **Giac bridge**: `backend/giac/` implements `MathBackend`; `Expr ⇄ giac::gen` conversion for all EXPR_SPEC heads; first operations: Integrate, Limit, Series, Solve, Factor, Simplify, DSolve, linear algebra | backend | Jules | Codex | todo | T-013, T-014 | | 2026-10-05 |
| T-013 | Verification layer: check integrals (differentiate), solutions (substitute), ODE solutions (substitute + conditions), matrix results; numeric fallback check with Arb/Boost; result status `verified`/`numeric`/`unverified` | core | Jules | Codex | todo | app result display | | 2026-10-05 |
| T-014 | Backend comparison suites: Rubi integral test set (MIT) and SymPy cases (BSD) run through Symats in CI; report mismatches — merged (PR #4) | tests | Jules | Copilot | done | | backup-ok | 2026-10-02 |
| T-015 | SUNDIALS: add to `third_party/` (Copilot builds) and `backend/sundials/` NDSolve adapter (Claude) | backend | Jules | Copilot | todo | numeric plots of ODEs | | 2026-10-05 |
| T-016 | App shell — **superseded by T-019** (bare notebook UI per AGENTS.md "Notebook model") | app | Codex | Claude | blocked | | | 2026-10-02 |
| T-017 | **Merged into T-024** (pattern syntax is part of Tier 1 parsing) | convert | Codex | Claude | done | | backup-ok | 2026-10-02 |
| T-018 | Flat/Orderless-aware matching so `a_ + b_` matches sums with any number of terms (core/pattern.cpp) — check behavior against **Mathics3** (GPL, reference only) | core | Jules | Codex | todo | | | 2026-10-05 |
| T-019 | **Bare notebook UI** per AGENTS.md "Notebook model": cells with In[n]/Out[n] gutter, Shift+Enter, multi-statement cells, inline errors, minimal top bar (run/abort/restart/workspace), hover-only controls, MathLive per-cell toggle, KaTeX output. Replace the current card layout — **build on CodeMirror 6** (input cells), MathLive (2-D), KaTeX + STIX Two (output), Plotly (plots) | app | Jules | Claude | todo | | | 2026-10-05 |
| T-020 | `parse_cell(text)` in convert/: split a cell into statements (newline / `;`, continuation inside brackets or after an operator), mark trailing-`;` suppression; parse `%`, `%%`, `%n` → `Out(...)`; `a; b` → CompoundExpression | convert | Jules | Claude | review | T-019 | backup-ok | 2026-10-05 |
| T-021 | Kernel: `core/session.h` — shared Context across cells, In/Out history, per-statement results with status/errors, Out builtin, CompoundExpression, restart, `user_symbols()` for the workspace panel | core | Jules | Codex | review | T-019, T-022 | | 2026-10-05 |
| T-022 | Engine bridge: run the C++ Session behind the app (Tauri command or WebAssembly), async evaluation with abort; JSON messages {cell statements} → {results: MathJSON + LaTeX + status + error} | bridge | Jules | Claude | todo | T-019 | | 2026-10-05 |
| T-023 | Scripts: `.sym` import/export and `symats-cli file.sym` runner (extends T-006) | cli | Jules | Claude | todo | | backup-ok | 2026-10-05 |
| T-024 | Parser: all **Tier 1 operators** per EXPR_SPEC §3.12 — `/.`, `.` (Dot vs decimal), `y'(t)` → `Derivative(1, y)(t)`, `&&`, `||`, prefix `!` vs postfix `!`, patterns `x_` `x_h` `x__` `x___` `_`, `clear` alias; printer emits the same operators | convert | Jules | Claude | review | T-019 | backup-ok | 2026-10-05 |
| T-025 | Tier 2 operators (engine + parser): `:>`, `//.`, `===`, `=.`, `/;`, `[[i]]`, `;;`, `++ += -=` — check behavior against **Mathics3** (GPL, reference only) | core+convert | Jules | Codex | todo | | | 2026-10-05 |
| T-026 | Port from Numerica (MIT, with attribution): Vegas Monte Carlo integration for NIntegrate of multiple integrals; error-tracking floats for numeric result status | core/numeric | Jules | Codex | todo | | | 2026-10-05 |
| T-027 | Jupyter kernel with **xeus** (BSD-3): new folder `kernel/`, wraps `symats::Session` + `parse_cell` (T-020) so Symats runs in JupyterLab / VS Code notebooks; returns text/plain + text/latex — **priority: gives a usable notebook (JupyterLab / VS Code) before T-019** | kernel | Jules | Codex | todo | | | 2026-10-05 |
| T-028 | Tests: Session (multi-cell, %, errors), Tier 1 operators, and D/Expand expected values from mathematics, SymPy, Maxima; fix T-014 data (1/3 not Rational(1,3), drop +C, Solve format {{x -> a}}); add **Maxima** as second oracle (expected values recorded in test files with source noted) | tests | Jules | Codex | review | | backup-ok | 2026-10-05 |
| T-029 | Quick NDSolve with **Boost.Odeint** (RK45/Dormand–Prince, Rosenbrock for stiff) in `backend/odeint/` + numeric special functions via **Boost.Math**; SUNDIALS (T-015) later | backend | Jules | Claude | todo | numeric ODE plots | backup-ok | 2026-10-05 |
| T-030 | Evaluator semantics audit vs **Mathics3** (GPL, reference only): attributes, Set/SetDelayed edge cases, Listable, Hold; translate its doc-tests for features Symats has into tests/test_mathics_semantics.cpp | tests | Jules | Codex | todo | | backup-ok | 2026-10-05 |
