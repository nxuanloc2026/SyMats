# Handoff — Jules

## Current task
T-001 Takeover (Evaluator, pattern matching, definitions, MathBackend interface). Status: review / PR ready.

## Notes
- Completed T-001 takeover for Claude:
  - Created `MathBackend` abstract interface and global registration functions in `core/include/symats/backend.h` and `core/src/backend.cpp`.
  - Implemented pattern matching (`_`, `__`, typed constraints like `x_Integer`) and rule replacement (`Rule`, `RuleDelayed`) in `core/include/symats/pattern.h` and `core/src/pattern.cpp`.
  - Implemented `Session` evaluator class in `core/include/symats/evaluator.h` and `core/src/evaluator.cpp` managing `Set` (`=`), `SetDelayed` (`:=`), rule evaluation, and dispatching math heads (`Integrate`, `Solve`, `DSolve`, `Limit`, `Series`, `Factor`, `Simplify`) to `MathBackend`.
  - Registered core sources in `core/CMakeLists.txt`.
  - Created `tests/test_evaluator.cpp` and registered in `tests/CMakeLists.txt`.
  - Verified build and test execution passing via `ctest`.

## Open questions
None.
