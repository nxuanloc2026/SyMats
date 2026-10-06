# Handoff — Jules

## NEXT TASKS
In order, one PR each:
1. **T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
2. **T-030** Mathics3 semantics doc-tests (reference only, GPL) — PR ready.

## Current task
T-006: `symats-cli` REPL with Tier 1 square bracket input/output (`parse_cell` -> `Session` -> `to_text`).
Branch: `jules/T-006-cli-repl`
Status: review / PR ready.

## Notes on T-006
- Implemented interactive REPL loop in `cli/main.cpp` using `symats::Session`, `symats::parse_cell`, and `symats::to_text`.
- Linked `symats::convert` in `cli/CMakeLists.txt`.
- Handled `In[n]:=` prompt, multi-line bracket continuations, `Out[n]=` output, inline error reporting, and exit commands (`Quit`/`Exit`).
- Verified via CTest and interactive pipe test execution.

## Completed tasks
- **T-009**: Optional GMP backend for Integer behind SYMATS_USE_GMP option (PR #12 merged).
