# Handoff — Jules

## TAKEOVER & REASSIGNMENT NOTICE (2026-10-05)
All remaining tasks on the task board have been reassigned to **Jules**.

## Tasks completed recently:
- **T-009**: Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP`.
- **T-020**: `parse_cell(text)` in `convert/` for statement splitting, `;` suppression, and history shortcuts (`%`, `%%`, `%n`).
- **T-006**: Interactive REPL and `.sym` script file execution in `symats-cli`.
- **T-010**: Native symbolic differentiation (`D`) and polynomial expansion (`Expand`) in `core/`.

## Next Open Tasks:
1. **T-024**: Parser: all Tier 1 operators per EXPR_SPEC §3.12 (`/.`, `y'(t)`, `&&`, `||`, prefix/postfix `!`, patterns `x_`, `x_h`, `x__`).
2. **T-030**: Evaluator semantics audit vs Mathics3: doc-tests in `tests/test_mathics_semantics.cpp`.
3. **T-018**: Flat / Orderless-aware pattern matching in `core/pattern.cpp`.
4. **T-025**: Tier 2 operators (`:>`, `//.`, `===`, `=.`, `/;`, `[[i]]`, `;;`, `++`, `+=`, `-=`).
5. **T-029**: Quick `NDSolve` with Boost.Odeint & Boost.Math in `backend/odeint/`.
6. **T-026**: Port Vegas Monte Carlo integration from Numerica (MIT) for `NIntegrate`.
