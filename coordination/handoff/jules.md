# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each: **T-028** tests (incl. T-014 data fixes, Maxima as second oracle),
**T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
**T-009** optional GMP backend, **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-028: Tests: Session (multi-cell, %, errors), Tier 1 operators, and D/Expand expected values from mathematics, SymPy, Maxima; fix T-014 data; Maxima as second oracle.
Branch: `jules/T-028-tests-maxima-oracle`
Status: review / PR ready

## Notes
- Fixed T-014 dataset issues in `tests/test_backend_comparison.cpp` (`1/3` representation, dropped `+C` from antiderivatives, updated `Solve` output format to `{{x -> a}}`).
- Added Maxima 5.47.0 oracle dataset (`maxima_cases` with 8 test cases across Integration, Differentiation, Expansion, Factorization, Limit, Solving, LinearAlgebra, DSolve with GPL source noted) and added `maxima_comparison_suite` test case.
- Added test cases in `test_session.cpp` for multi-cell pipeline error recovery and history shortcut (`%`, `Out(-1)`, `%1`).
- Added test cases in `test_operators.cpp` for rule lists with `ReplaceAll` and relational operators.
- Added test cases in `test_eval.cpp` for `D` and `Expand` expected structural representations.
- Verified all tests pass cleanly via `ctest`.

## Open questions
None.
