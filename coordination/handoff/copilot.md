# Handoff — GitHub Copilot

## Current task
T-013 native backend result verification. The implementation is in
`core/include/symats/verification.h` and `core/src/verification.cpp`.

## Notes
- Bridge sources are in `backend/giac/`: conversion, backend dispatch, and ODE
  system handling.
- The Giac-enabled workflow passed on PR #24 (run 37841664183) and on `main`
  (run 37842722557), including the `giac-backend` regression test.
- The verifier certifies indefinite integrals by differentiation, Solve/DSolve
  rule sets by substitution including initial conditions, Factor by expansion,
  and Det/Transpose/Inverse/LinearSolve by exact matrix identities.
- Giac marks verified results as `ResultStatus::Verified`; unsupported operations
  remain `Unverified`. Definite integrals, limits, series, eigen results, and
  MatrixExp are intentionally not certified yet because no sound native check is
  available in core.
- PR #25 contains the implementation and all required C/C++ and Giac Linux/Windows
  checks pass. The Arb/Boost numeric fallback portion of T-013 is deferred until a
  numeric result representation and numeric backend are available.

## Open questions
## Takeover: T-015 SUNDIALS/NDSolve

- Claude is unavailable, so Copilot is implementing the backend/build slice on branch
  `nxuanloc2026-t-015-sundials-ndsolve`.
- Existing `MathBackend` has no numeric value or interpolating-function representation.
  The adapter must therefore expose a focused, explicit `NDSolve` result shape without
  inventing core APIs; unsupported expression forms should decline.
- Next: add an optional pinned SUNDIALS dependency, CVODE adapter for scalar first-order
  ODEs, focused tests, and CI configuration.
