# Handoff — Jules

## NEXT TASKS
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-030** Mathics3 semantics doc-tests (reference only, GPL) — PR ready.

## Current task
T-027: Jupyter kernel with xeus (BSD-3): new folder `kernel/`, wraps `symats::Session` + Tier 1 square bracket `parse_cell` (T-020) so Symats runs in JupyterLab / VS Code notebooks; returns text/plain + text/latex.
Branch: `jules/T-027-xeus-jupyter-kernel`
Status: review / PR ready.

## Notes on T-027
- Implemented `parse_cell` in `convert/` for cell statement splitting, comment removal, line continuations, `;` output suppression, and `%` history expansion.
- Implemented `to_latex` in `convert/` for rendering Symats expressions in LaTeX notation (`\frac`, `\sqrt`, `\int`, matrices, math functions).
- Implemented `symats_interpreter` in `kernel/` extending `xeus::xinterpreter`, providing `text/plain` and `text/latex` outputs.
- Added `kernel/main.cpp`, `kernel/kernel.json.in`, and `kernel/CMakeLists.txt` with option `SYMATS_BUILD_KERNEL`.
- Added unit tests in `tests/test_text.cpp` covering `parse_cell` and `to_latex`.
- Verified 100% of unit tests pass cleanly via CTest.

## Completed tasks
- **T-009**: Optional GMP backend for Integer behind SYMATS_USE_GMP option (PR #12 merged).
