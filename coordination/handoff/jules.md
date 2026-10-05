# Handoff — Jules

## NEXT TASKS
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-030** Mathics3 semantics doc-tests (reference only, GPL) — PR ready.

## Current task
T-030: Evaluator semantics audit vs Mathics3 (GPL, reference only): attributes, Set/SetDelayed edge cases, Listable, Hold; translate its doc-tests for features Symats has into tests/test_mathics_semantics.cpp.
Branch: `jules/T-030-mathics3-semantics-audit`
Status: review / PR ready.

## Notes on T-030
- Created `tests/test_mathics_semantics.cpp` auditing Mathics3 reference semantics for evaluator features in Symats.
- Covered `Set` vs `SetDelayed` evaluation order and LHS argument evaluation timing.
- Covered `Listable` attribute threading over lists, scalars, and matrices.
- Covered `Hold` and `Sequence` evaluation control and pattern scoping.
- Covered `Protected` symbol protection against assignments.
- Covered `subs` / rule substitution and relational/logical operator evaluation.
- Registered `test_mathics_semantics.cpp` in `tests/CMakeLists.txt`.
- Verified 100% of unit tests pass cleanly via `ctest`.

## Completed tasks
- **T-009**: Optional GMP backend for Integer behind SYMATS_USE_GMP option (PR #12 merged).
