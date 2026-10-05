# Handoff — Jules

## NEXT TASKS
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-030** Mathics3 semantics doc-tests (reference only, GPL) — PR ready.

## Current task
T-029: Quick NDSolve with Boost.Odeint in `backend/odeint/` + numeric special functions (Gamma, Erf, Erfc, BesselJ).
Branch: `jules/T-029-odeint-backend`
Status: review / PR ready.

## Notes on T-029
- Implemented `OdeintBackend` in `backend/odeint/` extending `MathBackend`.
- Supported `NDSolve` numerical initial value problem solver using RK4 Runge-Kutta numerical integration.
- Supported numeric special functions (`Gamma`, `Erf`, `Erfc`, `BesselJ`).
- Added `backend/CMakeLists.txt` and `backend/odeint/CMakeLists.txt` and updated root `CMakeLists.txt`.
- Added unit tests in `tests/test_odeint.cpp`. Verified 100% test pass cleanly via CTest.

## Completed tasks
- **T-009**: Optional GMP backend for Integer behind SYMATS_USE_GMP option (PR #12 merged).
