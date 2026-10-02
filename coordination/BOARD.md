# Task board

Statuses: `todo` → `in-progress` → `review` (PR open) → `done`; also `blocked`.
`Blocks` = tasks that cannot start until this one is done (takeover priority).
`backup-ok` = a backup may start this task while the owner is out.
`Updated` must be refreshed at every checkpoint (UTC). Stale after 12 h.

| ID | Task | Lane | Owner | Backup | Status | Blocks | Flags | Updated (UTC) |
|----|------|------|-------|--------|--------|--------|-------|---------------|
| T-008 | Install Git + C++ workload; build in VS 2022; push repo to GitHub | setup | Loc | — | todo | T-004, T-005, all cloud work | | 2026-10-01 |
| T-003 | Decide: `=` means assignment (Set) or equation (Equal)? **Decided: `=` → `Set`, `==` → `Equal`, `:=` → `SetDelayed`** (spec updated) | spec | Loc | — | done | | | 2026-10-02 |
| T-001 | Evaluator: definitions (`Set`, `SetDelayed`), pattern matching (`x_`, `x__`, `x_Integer`), rule replacement, attributes; **plus `MathBackend` interface in core/ and dispatch of math heads (`Integrate`, `Solve`, `DSolve`, …) to the registered backend** | core | Claude | Codex | todo | T-010, T-012 | | 2026-10-02 |
| T-002 | Converter fixes: (1) nesting-depth limit instead of crash, (2) readable output (`x - y`, `-x^2`, `sin(x)`, `==`, `->`), (3) decimals, (4) document/decide space-before-paren, (5) `=` per T-003 | convert | Codex | Claude | done | | backup-ok | 2026-10-02 03:38 |
| T-004 | Verify MSVC build + GitHub Actions CI green on Windows and Linux; fix warnings | build/CI | Copilot | Codex | todo | | backup-ok | 2026-10-01 |
| T-005 | Tests: parser edge cases (unicode, whitespace, huge numbers, malformed input), Integer/Rational property tests | tests | Jules | Copilot | todo | | backup-ok | 2026-10-01 |
| T-006 | `symats-cli` REPL: read a line → `parse_text` → print `to_text` | cli | Copilot | Codex | todo | | backup-ok | 2026-10-01 |
| T-007 | MathJSON ⇄ Expr converter | convert | Codex | Claude | in-progress | app editor | | 2026-10-02 04:04 |
| T-009 | Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP` | core | Claude | Copilot | todo | | backup-ok | 2026-10-01 |
| T-010 | **Native** `D` (partial derivatives, chain/product rule, all elementary functions) and `Expand` in core/ — no Giac; these power result verification | core | Claude | Codex | todo | T-013 | | 2026-10-02 |
| T-011 | Giac build: fork Giac under nxuanloc2026, pin a release, add as git submodule in `third_party/giac` (+ GMP via vcpkg), CMake target `third_party::giac`, building on Windows (MSVC) and Linux CI | third_party/build | Copilot | Codex | todo | T-012 | | 2026-10-02 |
| T-012 | **Giac bridge**: `backend/giac/` implements `MathBackend`; `Expr ⇄ giac::gen` conversion for all EXPR_SPEC heads; first operations: Integrate, Limit, Series, Solve, Factor, Simplify, DSolve, linear algebra | backend | Claude | Codex | todo | T-013, T-014 | | 2026-10-02 |
| T-013 | Verification layer: check integrals (differentiate), solutions (substitute), ODE solutions (substitute + conditions), matrix results; numeric fallback check with Arb/Boost; result status `verified`/`numeric`/`unverified` | core | Claude | Codex | todo | app result display | | 2026-10-02 |
| T-014 | Backend comparison suites: Rubi integral test set (MIT) and SymPy cases (BSD) run through Symats in CI; report mismatches | tests | Jules | Copilot | todo | | backup-ok | 2026-10-02 |
| T-015 | SUNDIALS: add to `third_party/` (Copilot builds) and `backend/sundials/` NDSolve adapter (Claude) | backend | Claude | Copilot | todo | numeric plots of ODEs | | 2026-10-02 |
| T-016 | App shell: Tauri desktop app with MathLive editor cell, text toggle, Plotly plot cell; talks to engine through `bridge/` | app | Codex | Claude | todo | | | 2026-10-02 |
