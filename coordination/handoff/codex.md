# Handoff — Codex

## Reconcile Jules completion report (2026-10-06 18:35 UTC)

Loc reports that Jules finished the work. Fetched all remote branches and
merged origin/main (6aa447b) into the local T-012 branch, resolving coordination
conflicts without dropping the bridge. T-011 PR #22 is merged and all three
workflows passed at 1dca42e, including Windows/Linux Giac builds. The published
Jules handoff and remote branches do not yet contain T-012/T-013/T-015/T-026;
requested the Jules task or PR link to identify and review the newer work.
T-012 is preserved locally at 1f9acf4 with its six passing sanitizer test cases;
no duplicate PR has been opened. Further overlapping implementation awaits
identification of Jules's result. T-013/T-015/T-026 have not been implemented here.

## T-011 checkpoint (2026-10-06 13:28 UTC)

The fork pin 6ba6b31 also fixes the inverse-Laplace temporary-identifier
use-after-free found by the T-012 system tests under AddressSanitizer. The
smoke test now calls inverse Laplace twice in the same context. Linux CI is
green at a015f6b. Windows failed before compilation twice because GNU/GMP
downloads timed out; CI now seeds the archives from kernel.org with the exact
SHA512 hashes from the pinned vcpkg portfiles, and caches downloads/binaries.

## T-012 checkpoint (2026-10-06 13:24 UTC)

All six Giac bridge test cases pass under MSVC AddressSanitizer Debug, including
exact round-trips, calculus/algebra, scalar and coupled ODEs, matrix operations,
malformed input, fresh ODE constant names, and evaluator registration. Removed
the temporary Windows assertion tracer. The system adapter uses Giac's Laplace
and linear solvers for first-order constant-coefficient systems; its limits are
documented in backend/giac/README.md. One additional fork fix in
signalprocessing.cc prevents a repeated inverse-Laplace use-after-free; fold
that pin and regression into T-011 before final bridge PR. Linux T-011 CI passed
again; Windows GMP downloads repeatedly time out. A kernel.org mirror matches
the pinned vcpkg port's SHA512 and will be used to seed its download cache.

## T-012 Giac bridge (2026-10-05)

Claimed `codex/T-012-giac-bridge` from the T-011 branch at `6240a8d` while
PR #22 runs hosted CI. Implement `backend/giac/` only, with Expr/gen conversion
and `MathBackend` dispatch for the board's operations. Keep the PR stacked on
T-011 until that PR lands, then rebase on main. No existing public core API or
EXPR_SPEC change is planned; any unavoidable change will be explained in
`coordination/handoff/claude.md`.

Checkpoint 2026-10-06 04:35 UTC: the bridge now converts exact atoms,
mapped/generic heads, and Giac inverse nodes. Factor, indefinite Integrate,
Limit, Solve, and Det execute under MSVC AddressSanitizer Debug after fixing
upstream alias and cross-container iterator bugs. The bridge is still WIP:
Solve needs rule-shaped output, Series needs an explicit remainder contract,
DSolve and the remaining matrix operations need conversion/tests, and temporary
assertion tracing must be removed. The pending Giac fork changes are in
`src/gausspol.cc` and `src/vecteur.cc`. T-011 Windows CI is green; Linux needs
`HAVE_PWD_H` enabled before rerunning. No T-012 PR is ready yet.
## Build and bridge crash fixes (2026-10-06)

T-011 Linux CI passed at f58c29b; its Windows run stopped at a GMP download
timeout (the preceding Windows Debug run passed). The fork pin c9e056c fixes
additional checked-iterator errors exercised by ODEs, small matrix products,
characteristic polynomials, and eigenvalues. The new smoke regressions cover
those operations. T-012 is checkpointed at dd89a98; scalar ODEs, integration,
algebraic solves, and matrix operations now run under MSVC AddressSanitizer
Debug without a crash. Remaining bridge work: linear ODE system adaptation,
fresh generated constant names, remove temporary tracing, full suite and PR.

## T-011 Giac build (2026-10-05)

Claimed `codex/T-011-giac-build` from current main `6c41bda` after PR #21 merged.
The user requested T-011, T-012, T-013, T-015, and T-026, one branch and PR
per task. T-011 blocks T-012, which blocks the full T-013 verification path.
Plan: pin a GPL-3+ Giac release in a fork and submodule, provide
`third_party::giac`, and prove MSVC and Linux builds with GMP. The source
currently uses Autotools and old Visual C++ build files; assess this before
choosing the integration. No public core API or expression-spec change is
planned for T-011.

## T-010 native calculus takeover (2026-10-05)

Completed T-010 on `takeover/T-010-native-calculus` from main `4db3660`.
PR: https://github.com/nxuanloc2026/Symats/pull/21 (human merge required).
Added native `D` and `Expand` in `core/src/calculus.cpp`, a shared calculus header,
and evaluator dispatch.

## T-019 bare notebook UI

Created `app/` using CodeMirror 6, MathLive, Compute Engine, KaTeX, STIX Two Math, and Plotly.
Merged into `main`.

## NEW DIRECTION (2026-10-02, from Loc via Claude) — read first
Loc wants **Mathematica with a lighter UI**: a notebook with a live kernel where variables
persist across cells and cells contain multiple lines of code.

## Current task
None.
