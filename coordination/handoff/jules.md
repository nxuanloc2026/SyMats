# Handoff — Jules

## Current task
T-024: Parser: all Tier 1 operators per EXPR_SPEC §3.12.
Branch: `jules/T-024-tier1-operators`
Status: review / PR ready.

## Notes on T-024
- Implemented parser and printer support in `convert/src/text.cpp` for all Tier 1 operators:
  - ReplaceAll `/.`
  - Logic `||` (Or), `&&` (And), prefix `!` (Not)
  - Postfix `!` (Factorial)
  - Derivative prime syntax `y'(t)`, `f''(x)` -> `Derivative(1, y)(t)`, `Derivative(2, f)(x)`
  - Pattern expressions `x_`, `x_h`, `x__`, `x___`, `_`, `_h`, `__`, `___` -> `Pattern`, `Blank`, `BlankSequence`, `BlankNullSequence`
  - Alias `clear` -> `Clear`
- Added comprehensive unit tests in `tests/test_text.cpp`.

## Tasks completed recently:
- **T-009**: Optional GMP backend for `Integer` behind CMake option `SYMATS_USE_GMP`.
- **T-020**: `parse_cell(text)` in `convert/` for statement splitting, `;` suppression, and history shortcuts (`%`, `%%`, `%n`).
- **T-006**: Interactive REPL and `.sym` script file execution in `symats-cli`.
- **T-010**: Native symbolic differentiation (`D`) and polynomial expansion (`Expand`) in `core/`.

## Next Open Tasks:
1. **T-030**: Evaluator semantics audit vs Mathics3: doc-tests in `tests/test_mathics_semantics.cpp`.
2. **T-018**: Flat / Orderless-aware pattern matching in `core/pattern.cpp`.
3. **T-025**: Tier 2 operators (`:>`, `//.`, `===`, `=.`, `/;`, `[[i]]`, `;;`, `++`, `+=`, `-=`).
4. **T-029**: Quick `NDSolve` with Boost.Odeint & Boost.Math in `backend/odeint/`.
5. **T-026**: Port Vegas Monte Carlo integration from Numerica (MIT) for `NIntegrate`.
