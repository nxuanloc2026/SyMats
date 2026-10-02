# Handoff — Claude

## Current task
None in progress. Next: T-001 (evaluator and pattern matching).

## Done
- v0.1 core: `Integer`, `Rational`, `Expr`, canonical `plus`/`times`/`power`, ordering,
  hashing, full-form printer, tests (`tests/test_integer.cpp`, `test_rational.cpp`, `test_expr.cpp`).
- Fixed grouping bug: numeric multiple of a sum is distributed only inside sums
  (`core/src/canonical.cpp`, `collect_terms`).

## Plan for T-001 (so a backup can start it)
1. `core/include/symats/eval.h`: `ExprPtr evaluate(const ExprPtr&, Context&)`; `Context` holds
   per-symbol own-values and down-values (rule lists).
2. `core/src/match.cpp`: `match(pattern, expr, Bindings&)` supporting `Blank` (`x_`),
   `BlankSequence` (`x__`), head-restricted blanks (`x_Integer`), with backtracking for
   sequences; `Orderless` heads try permutations later (not in first PR).
3. `replace_all(expr, rules)`; evaluation repeats until the result is unchanged (cap iterations).
4. Attributes: `HoldAll`, `HoldFirst`, `Listable`, `Flat`, `Orderless` (store, honor HoldAll first).
5. Tests: `fact(0)=1; fact(n_):=n*fact(n-1); fact(20)`.

## Open questions
- T-003 decided: `=` → `Set`, `==` → `Equal`, `:=` → `SetDelayed`. Evaluator handles
  `Set`/`SetDelayed` nodes; text syntax is Codex's T-002.
