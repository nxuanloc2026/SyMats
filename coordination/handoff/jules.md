# Handoff — Jules

## Current task
T-005: Tests for parser edge cases and Integer/Rational properties. Status: completed / review.

## Notes
- Created `tests/test_edge_cases.cpp` and registered in `tests/CMakeLists.txt`.
- Added test cases covering:
  - `parse_text` with unusual whitespace (tabs, newlines, mixed spacing around operators, calls, matrices, and rules).
  - Very long integers (500-digit, 1000-digit integers, arithmetic, multiplication, division).
  - Malformed input throwing `std::invalid_argument` without crashing (empty input, unbalanced delimiters, invalid operators, bad numbers, unsupported symbols).
  - Algebraic properties of `Integer` and `Rational` ($a+b-b=a$, $(a \cdot b)/b=a$, $\gcd(a, b)$ divides $a$ and $b$) tested over generated values using deterministic PRNG.
- All tests pass via `ctest`.

## Open questions
None.
