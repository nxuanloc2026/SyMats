# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each:
1. **T-009** optional GMP backend (in review / PR ready),
2. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
3. **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-009: Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP`.
Status: review / PR ready.

## Notes
- Added `SYMATS_USE_GMP` option in root `CMakeLists.txt` and `core/CMakeLists.txt`.
- Implemented `SYMATS_USE_GMP` support in `core/include/symats/integer.h` and `core/src/integer.cpp` using `mpz_class` while preserving public API. Fixed `long long` initialization via string conversion to avoid 64-bit integer truncation on 32-bit `long` platforms.
- Added algebraic property tests in `tests/test_integer.cpp`.
- Cleaned up build artifacts (`build_gmp/` removed) so git index stays clean.
- Verified test suite compiles and passes under default and `SYMATS_USE_GMP=ON` builds.

## Open questions
None.
