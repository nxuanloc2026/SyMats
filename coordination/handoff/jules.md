# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each: **T-028** tests (incl. T-014 data fixes, Maxima as second oracle) — submitted,
**T-018** Flat/Orderless-aware pattern matching — review / PR ready,
**T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
**T-009** optional GMP backend, **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-018: Flat/Orderless-aware matching so `a_ + b_` matches sums with any number of terms (`core/pattern.cpp`).
Branch: `jules/T-018-flat-orderless-matching`
Status: review / PR ready

## Notes
- Implemented Flat (associative) and Orderless (commutative) pattern matching algorithms in `core/src/pattern.cpp` and `core/include/symats/pattern.h`.
- Enabled Flat and Orderless attributes by default on `Plus` and `Times` expressions, with support for custom head attributes.
- Added comprehensive test cases in `tests/test_pattern.cpp` covering multivariable sum/product matching, repeated pattern names, custom orderless heads, and rule replacement with `replace_all`.
- Verified all tests pass via `ctest`.

## Open questions
None.
