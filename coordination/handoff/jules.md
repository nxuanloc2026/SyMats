# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each:
1. **T-030** Mathics3 semantics doc-tests (in review / PR ready),
2. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
3. **T-009** optional GMP backend.

## Current task
T-030: Evaluator semantics audit vs Mathics3 (translate doc-tests into tests/test_mathics_semantics.cpp).
Status: review / PR ready.

## Notes
- Created `tests/test_mathics_semantics.cpp` translating Mathics3 evaluator doc-tests for `Set`, `SetDelayed`, `Hold`, `Listable`, `CompoundExpression`, and `ReplaceAll`.
- Registered `test_mathics_semantics.cpp` in `tests/CMakeLists.txt`.
- Verified all unit tests pass cleanly via `ctest`.

## Open questions
None.
