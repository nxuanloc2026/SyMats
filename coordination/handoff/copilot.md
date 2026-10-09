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
_(Copilot: keep this file updated at every checkpoint.)_

## T-018 takeover
- Claimed T-018 because Claude is out of usage and the task is in the core lane.
- Branch: `nxuanloc2026-t018-flat-orderless-pattern-matching`.
- Implemented Flat/Orderless matching for `Plus` and `Times` in `core/src/pattern.cpp`.
- Added focused `Plus` tests covering multi-term bindings, literal-plus-pattern matching, and arity failure.
- Next: configure/build and run the pattern/evaluator tests; then review edge cases and open the PR.
