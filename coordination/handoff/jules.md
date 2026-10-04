# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-009** optional GMP backend,
3. **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-028: Tests for Session, Tier 1 operators, calculus expected values, T-014 data fix, Maxima oracle.
Status: review / PR ready.

## Notes
- Updated `tests/test_backend_comparison.cpp` with T-014 data fixes (Rational nodes, dropped +C, Rule output format for `Solve`) and added Maxima 5.46.0 test oracle suite.
- Added session multi-cell, symbol clearing, line numbering, and workspace symbol tracking test cases to `tests/test_session.cpp`.
- Added test cases in `tests/test_operators.cpp` for Tier 1 operators (Derivative, Patterns, Logic, ReplaceAll, Dot) and calculus expected value full forms (`D`, `Expand`, `Integrate`, `Solve`).
- All tests build and pass cleanly under CMake and `ctest`.

## Open questions
None.
