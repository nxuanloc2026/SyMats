# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-009** optional GMP backend,
3. **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-010: Native `D` (differentiation) and `Expand` in `core/`.
Branch: `jules/T-010-native-d-expand`
Status: review / PR ready.

## Notes on T-010
- Created `core/include/symats/calculus.h` and `core/src/calculus.cpp` implementing native `differentiate` and `expand`.
- Implemented sum linearity, n-factor product rule, general power rule, chain rule for all elementary functions (`Sin`, `Cos`, `Tan`, `Cot`, `Sec`, `Csc`, `ArcSin`, `ArcCos`, `ArcTan`, `Sinh`, `Cosh`, `Tanh`, `Exp`, `Log`, `Abs`), unknown functions (`Derivative(1, f)(u)`), and higher-order derivatives.
- Implemented `expand` distributing `Times` over `Plus` and expanding integer powers of sums.
- Registered built-in heads `D`, `Expand`, and `Derivative` in `core/src/eval.cpp`.
- Added unit tests in `tests/test_eval.cpp`.

## Notes
- Updated `tests/test_backend_comparison.cpp` with T-014 data fixes (Rational nodes, dropped +C, Rule output format for `Solve`) and added Maxima 5.46.0 test oracle suite.
- Added session multi-cell, symbol clearing, line numbering, and workspace symbol tracking test cases to `tests/test_session.cpp`.
- Added test cases in `tests/test_operators.cpp` for Tier 1 operators (Derivative, Patterns, Logic, ReplaceAll, Dot) and calculus expected value full forms (`D`, `Expand`, `Integrate`, `Solve`).
- All tests build and pass cleanly under CMake and `ctest`.
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
