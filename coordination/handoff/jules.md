# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each: **T-028** tests (incl. T-014 data fixes, Maxima as second oracle),
**T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
**T-009** optional GMP backend, **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-014: Backend comparison suites (Rubi integral test set and SymPy cases). Status: review / PR ready.

## Notes
- Created `tests/test_backend_comparison.cpp` and registered in `tests/CMakeLists.txt`.
- Added dataset structures and test cases for:
  - **Rubi integral cases** (MIT license): basic polynomial, rational, trigonometric, exponential, logarithm, inverse trigonometric integrals.
  - **SymPy test cases** (BSD 3-Clause license): differentiation, expansion, factorization, simplification, linear algebra (determinant, trace), and equation solving representation.
- Implemented `run_comparison_suite` harness that parses input/expected expressions, evaluates them (comparing actual vs. expected), and logs detailed reports on mismatches whenunevaluated operations remain or backend results differ.
- Verified build and test suite passing via `ctest`.

## Open questions
None.
