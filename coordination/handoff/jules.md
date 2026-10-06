# Handoff — Jules

## NEXT TASKS
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-030** Mathics3 semantics doc-tests (reference only, GPL) — PR ready.

## Current task
T-010: Native `D` (partial derivatives, chain/product rule, elementary functions) and `Expand` in `core/`.
Branch: `jules/T-010-native-d-expand`
Status: review / PR ready.

## Notes on T-010
- Implemented `diff` and `expand` functions in `core/include/symats/calculus.h` and `core/src/calculus.cpp`.
- Implemented symbolic partial differentiation with linearity, product rule, general power rule, chain rule for elementary functions (`Sin`, `Cos`, `Tan`, `Exp`, `Log`, `ArcSin`, `ArcCos`, `ArcTan`, `Sinh`, `Cosh`, `Tanh`, `Abs`), higher-order derivatives (`{x, n}`), and generic function derivatives (`Derivative[1][f][u]`).
- Implemented `Expand` for polynomial distribution and positive integer powers.
- Registered built-ins `"D"` and `"Expand"` in `core/src/eval.cpp`.
- Added unit tests in `tests/test_calculus.cpp`. Verified 100% tests pass cleanly via CTest.

## Completed tasks
- **T-009**: Optional GMP backend for Integer behind SYMATS_USE_GMP option (PR #12 merged).
