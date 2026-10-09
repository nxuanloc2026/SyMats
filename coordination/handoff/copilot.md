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
## Takeover: T-015 SUNDIALS/NDSolve

- Claude is unavailable, so Copilot is implementing the backend/build slice on branch
  `nxuanloc2026-t-015-sundials-ndsolve`.
- Existing `MathBackend` has no numeric value or interpolating-function representation.
  The adapter must therefore expose a focused, explicit `NDSolve` result shape without
  inventing core APIs; unsupported expression forms should decline.
- Next: add an optional pinned SUNDIALS dependency, CVODE adapter for scalar first-order
  ODEs, focused tests, and CI configuration.

## Handoff: PR #30

- Implemented and pushed commit `8c737a0` on `nxuanloc2026-t-015-sundials-ndsolve`.
- PR: https://github.com/nxuanloc2026/Symats/pull/30
- Local `git diff --check` passes. CMake/SUNDIALS are unavailable in this environment;
  CI is configured to install `libsundials-dev` on Ubuntu and `sundials:x64-windows`
  through vcpkg.
- The first dependency-enabled CI run exposed portability issues (`realtype`,
  `SUN_COMM_NULL`, and the Debian SUNDIALS core library name); these are fixed in
  commits `d2f7ff0` and `dd035ca`. The previous run's Giac job is still the failed
  check for the older commit; no new check was reported yet for `dd035ca`.
- The adapter intentionally supports only scalar first-order ODEs until the core has
  a floating-point/interpolating-function representation.
_(Copilot: keep this file updated at every checkpoint.)_

## T-018 takeover
- Claimed T-018 because Claude is out of usage and the task is in the core lane.
- Branch: `nxuanloc2026-t018-flat-orderless-pattern-matching`.
- Implemented Flat/Orderless matching for `Plus` and `Times` in `core/src/pattern.cpp`.
- Added focused `Plus` tests covering multi-term bindings, literal-plus-pattern matching, and arity failure.
- Local CMake/compiler tools are unavailable, so `git diff --check` is the local validation performed.
- Committed as `f3b5fff`, pushed branch `nxuanloc2026-t018-flat-orderless-pattern-matching`, and opened PR #29.
- CI is the remaining build/test validation.
## T-006 checkpoint
- Existing implementation from Jules provides the interactive REPL and `.sym` runner.
- Follow-up hardening is in `cli/main.cpp`: shared input execution, whitespace-tolerant
  `quit`/`exit`, and preserved nonzero script exit status on evaluation errors.
- README now documents REPL and script usage.
- Native CMake validation is blocked in this environment because `cmake` and C++
  compilers are not on PATH; `git diff --check` passes.
- Committed as `e550173`, pushed as `nxuanloc2026-t006-cli-repl`, and opened PR #28.
