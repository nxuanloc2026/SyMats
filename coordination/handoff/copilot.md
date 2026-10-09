# Handoff — GitHub Copilot

## Current task
T-020 `parse_cell` reconciliation. The implementation is already present on main
in `convert/src/text.cpp`.

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
- T-020 is implemented by main commits `c29ef70` and `4231cc4`; `tests/test_text.cpp`
  covers comments, multi-statement cells, continuations, semicolon suppression, and
  `%`/`%%`/`%n` history expansion. No duplicate implementation is needed.

## Open questions
_(Copilot: keep this file updated at every checkpoint.)_
