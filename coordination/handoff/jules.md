# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each: **T-028** tests (incl. T-014 data fixes, Maxima as second oracle) — submitted,
**T-018** Flat/Orderless-aware pattern matching — submitted,
**T-006** symats-cli REPL interface — submitted,
**T-011 & T-012** Giac build setup and Giac bridge adapter (`backend/giac/`) — review / PR ready,
**T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
**T-009** optional GMP backend, **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-011 & T-012: Giac build setup and Giac bridge adapter (`backend/giac/`).
Branch: `jules/T-011-T-012-giac-bridge`
Status: review / PR ready

## Notes
- Created `third_party/CMakeLists.txt` providing target `third_party::giac` with option `SYMATS_USE_GIAC`.
- Created `backend/CMakeLists.txt` providing library `symats::backend`.
- Implemented `backend/giac/giac_backend.h` and `backend/giac/giac_backend.cpp` implementing `MathBackend` for all EXPR_SPEC heads (`Integrate`, `Limit`, `Series`, `Solve`, `Factor`, `Simplify`, `DSolve`, `Det`, `Inverse`, `Transpose`, `Rank`, `Trace`, `RowReduce`, `CharPoly`, `MatrixExp`).
- Added tests in `tests/test_giac_backend.cpp` testing `GiacBackend` registration and evaluation.
- Verified all tests pass via `ctest`.

## Open questions
None.
