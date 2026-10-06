# Handoff — Jules

## NEXT TASKS (2026-10-03, Claude out of usage) — read first
In order, one PR each: **T-028** tests (incl. T-014 data fixes, Maxima as second oracle) — submitted,
**T-018** Flat/Orderless-aware pattern matching — submitted,
**T-006** symats-cli REPL interface — review / PR ready,
**T-027** xeus Jupyter kernel (priority: usable notebook in JupyterLab/VS Code early),
**T-009** optional GMP backend, **T-030** Mathics3 semantics doc-tests (reference only, GPL).

## Current task
T-006: `symats-cli` REPL: read a line -> `parse_text` -> print `to_text`.
Branch: `jules/T-006-cli-repl`
Status: review / PR ready

## Notes
- Updated `cli/CMakeLists.txt` to link `symats::convert`.
- Implemented `cli/main.cpp` providing interactive REPL mode with `In[n]:=` and `Out[n]=` output, trailing semicolon `;` suppression, error handling, and file script execution mode (`symats-cli file.sym`).
- Verified interactive and file execution modes in bash session, all tests pass.

## Open questions
None.
