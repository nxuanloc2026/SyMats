# Symats Expression Specification (v0.1)

This document is the contract between the engine (`core/`), the converter (`convert/`),
and the app (`app/`). Every agent must use these node names exactly.

## 1. The expression model

Every value in Symats is an **expression** (`Expr`). There are four kinds:

| Kind       | Meaning                              | Examples                  |
|------------|--------------------------------------|---------------------------|
| `Integer`  | exact arbitrary-precision integer    | `0`, `-7`, `123456789012345678901` |
| `Rational` | exact fraction p/q, q > 1, gcd = 1   | `1/2`, `-3/4`             |
| `Symbol`   | a name                               | `x`, `t`, `Pi`, `Plus`    |
| `Normal`   | a head applied to arguments          | `Plus[x, 1]`, `Integrate[f, x]` |

Later kinds (not in v0.1): `Real` (MPFR arbitrary precision), `String`.

Rules:
- Expressions are **immutable** and shared (`std::shared_ptr<const Expr>`).
- A `Normal` has a head (any Expr, usually a Symbol) and an ordered list of arguments.
- The **full form** below (`Head[arg1, arg2, …]`) is also valid plain-text input.
- Function arguments use square brackets, lists use curly braces, and parentheses only
  group arithmetic. Built-in names are capitalized and case-sensitive; user-defined
  functions may use lowercase names (`f[x_] := Sin[x]`).

## 2. Canonical form (engine guarantees)

The engine always returns expressions in canonical form, so equal math has equal trees.

| Rule | Example |
|------|---------|
| `Plus`/`Times` are flattened | `Plus[a, Plus[b, c]]` → `Plus[a, b, c]` |
| Numbers are combined | `Plus[2, x, 3]` → `Plus[5, x]` |
| Like terms are collected | `x + x` → `Times[2, x]`; `x*x` → `Power[x, 2]` |
| Numeric multiple of a sum is distributed only inside a sum | `2*(x+1)` stays `Times[2, Plus[1, x]]`; `y + 2*(x+1)` → `Plus[2, Times[2, x], y]` |
| Result never depends on grouping | `(2*s)*y` = `2*(s*y)` = `Times[2, s, y]` |
| Identities removed | `Plus[x, 0]` → `x`; `Times[1, x]` → `x`; `Times[0, x]` → `0` |
| Single argument unwrapped | `Plus[x]` → `x` |
| Arguments sorted (canonical order) | `Plus[y, x, 1]` → `Plus[1, x, y]` |
| Subtraction / division are not nodes | `a - b` = `Plus[a, Times[-1, b]]`; `a / b` = `Times[a, Power[b, -1]]` |
| Rational powers of numbers stay exact | `Power[4, 1/2]` → `2`; `Power[2, 1/2]` stays |

Canonical order: numbers first (by value), then symbols (alphabetical), then normals
(by head, then arguments). Powers of the same base sort next to the base.

## 3. Node names

Notation columns: **Text** = plain-text input/output; **2-D** = what the math editor shows.
Plain-text built-ins use the same capitalized, case-sensitive names as Expr heads.
`Sin[x]` and `sin[x]` are distinct; the latter is an ordinary user symbol.

### 3.1 Arithmetic

| Head | Arguments | Text | 2-D |
|------|-----------|------|-----|
| `Plus` | terms… | `a + b` | a + b |
| `Times` | factors… | `a*b`, `a b` | a·b |
| `Power` | base, exponent | `a^b` | aᵇ |
| `Sqrt` (input only → `Power[x, 1/2]`) | x | `Sqrt[x]` | √x |
| `Root` (input only → `Power[x, 1/n]`) | x, n | `Root[x, n]` | ⁿ√x |
| `Abs` | x | `Abs[x]` | \|x\| |
| `Factorial` | n | `n!` | n! |

### 3.2 Constants (Symbols)

`Pi` (π), `E` (e), `I` (i), `Infinity` (∞), `True`, `False`.
Text: `Pi`, `E`, `I`, `Infinity`, `True`, `False`.

### 3.3 Functions

`Sin Cos Tan Cot Sec Csc ArcSin ArcCos ArcTan Sinh Cosh Tanh Exp Log`
Text: `Sin[x]`, `ArcTan[x]`, `Log[x]` = natural log, `Log[b, x]` = base b.

### 3.4 Relations and logic

| Head | Text | 2-D |
|------|------|-----|
| `Equal` | `a == b` (a single `=` is assignment, see §3.10) | a = b |
| `Unequal` | `a != b` | a ≠ b |
| `Less`, `LessEqual`, `Greater`, `GreaterEqual` | `<  <=  >  >=` | <  ≤  >  ≥ |
| `And`, `Or`, `Not` | `a && b`, `a \|\| b`, `!a` | ∧ ∨ ¬ |
| `ReplaceAll` | `expr /. x -> 3`, `expr /. {x -> 1, y -> 2}` | |
| `List` | `{a, b, c}` | {a, b, c} |
| `Rule` | `x -> 2` | x → 2 |

### 3.5 Calculus

| Head | Arguments | Text | 2-D |
|------|-----------|------|-----|
| `D` | f, x | `D[f, x]` | d/dx f |
| `D` | f, {x, n} | `D[f, {x, n}]` | dⁿ/dxⁿ f |
| `D` | f, x, y | `D[f, x, y]` | ∂²f/∂x∂y |
| `Derivative` | `Derivative[n][f]` used as a head: `y'[t]` = `Derivative[1][y][t]`, `y''[t]` = `Derivative[2][y][t]` | `y'[t]`, `f''[x]` | y′, y″ |
| `Integrate` | f, x | `Integrate[f, x]` | ∫ f dx |
| `Integrate` | f, {x, a, b} | `Integrate[f, {x, a, b}]` | ∫ₐᵇ f dx |
| `Integrate` | f, {x, a, b}, {y, c, d} | `Integrate[f, {x, a, b}, {y, c, d}]` | ∬ f dx dy (limits may depend on outer variables) |
| `Limit` | f, Rule(x, a) | `Limit[f, x -> a]` | lim_{x→a} f |
| `Sum` | f, {k, a, b} | `Sum[f, {k, a, b}]` | Σ |
| `Product` | f, {k, a, b} | `Product[f, {k, a, b}]` | Π |
| `Series` | f, {x, a, n} | `Series[f, {x, a, n}]` | Taylor series |

`Series` results use `SeriesData[terms, {x, a, n}]`: a formal expansion about
`x = a`, truncated through the requested order `n`. The wrapper retains the
truncation metadata; `terms` alone must not be presented as equal to the original
function. This is a named head using the existing Tier 1 call syntax.

### 3.6 Differential equations

| Head | Meaning | Text example |
|------|---------|--------------|
| `DSolve` | symbolic solve | `DSolve[y''[x] + y[x] == 0, y[x], x]` |
| `DSolve` (system) | eqns list, funcs list, var | `DSolve[{x'[t] == y[t], y'[t] == -x[t]}, {x[t], y[t]}, t]` |
| `DSolve` (PDE) | eqn, u[x,t], {x, t} | `DSolve[D[u[x,t], t] == k*D[u[x,t], {x, 2}], u[x,t], {x,t}]` |
| `NDSolve` | numeric solve | `NDSolve[{y'[x] == -y[x], y[0] == 1}, y, {x, 0, 5}]` |
| `NIntegrate` | numeric integral | `NIntegrate[Exp[-x^2], {x, 0, Infinity}]` |

Results are `List` of `Rule`s: `{{y[x] -> C1*Cos[x] + C2*Sin[x]}}`.
Numeric results return `InterpolatingFunction[…]` usable in `Plot`.

### 3.7 Matrices and linear algebra

A matrix is a `List` of row `List`s; a vector is a `List`.

| Head | Text | 2-D |
|------|------|-----|
| (matrix) | `{{a, b}, {c, d}}` | bracketed grid template |
| `Dot` | `A . B` (vector·vector → scalar, matrix·vector, vector·matrix, matrix·matrix) | A·B |
| `Transpose` | `Transpose[A]` | Aᵀ |
| `Det` | `Det[A]` | \|A\| or det A |
| `Inverse` | `Inverse[A]` | A⁻¹ |
| `Eigenvalues`, `Eigenvectors` | `Eigenvalues[A]` | |
| `Rank`, `Trace`, `RowReduce`, `CharPoly`, `MatrixExp` | capitalized built-in calls | |
| `LinearSolve` | `LinearSolve[A, b]` | |

### 3.8 Algebra

`Expand`, `Factor`, `Simplify`, `Together`, `Apart`, `Solve`, `Substitute`
Text: `Expand[e]`, `Factor[e]`, `Simplify[e]`, `Solve[eqn, x]`, `Substitute[e, x -> 2]`.

### 3.9 Plotting

| Head | Text example |
|------|--------------|
| `Plot` | `Plot[Sin[x], {x, 0, 2*Pi}]`; multiple: `Plot[{Sin[x], Cos[x]}, {x, 0, 2*Pi}]` |
| `ParametricPlot` | `ParametricPlot[{Cos[3t], Sin[2t]}, {t, 0, 2*Pi}]` |
| `PolarPlot` | `PolarPlot[1 + Cos[th], {th, 0, 2*Pi}]` |
| `ImplicitPlot` | `ImplicitPlot[x^2 + y^2 == 1, {x, -2, 2}, {y, -2, 2}]` |
| `Plot3D` | `Plot3D[Sin[x]*Cos[y], {x, -3, 3}, {y, -3, 3}]` |
| `ContourPlot` | `ContourPlot[x^2 - y^2, {x, -2, 2}, {y, -2, 2}]` |
| `Animate` | `Animate[Plot[Sin[x - t], {x, 0, 2*Pi}], {t, 0, 10}]` |
| `Slider` | `Plot[Sin[a*x], {x, 0, 2*Pi}, Slider[a, 0, 5]]` |

### 3.10 Assignment

| Head | Text | Meaning |
|------|------|---------|
| `Set` | `f = expr` | assign now |
| `SetDelayed` | `f[x_] := x^2` | define rule, evaluate on use |
| `Pattern` | `x_` | matches anything, binds `x` |
| `Clear` | `Clear[f]` | remove own value and definitions |
| `CompoundExpression` | `a; b; c` | evaluate in order, return the last (`a; b;` → `Null`) |
| `Out` | `%`, `%%`, `%5`, `Out[5]` | previous outputs in the session (`Out[]` = last, `Out[-2]` = second to last) |

Pattern nodes (implemented in `core/pattern.h`, T-001):

| Text | Internal form | Matches |
|------|---------------|---------|
| `_` | `Blank[]` | any one expression |
| `_Integer` | `Blank[Integer]` | one integer |
| `x_`, `x_Integer` | `Pattern[x, Blank[]]`, `Pattern[x, Blank[Integer]]` | same, binding `x` |

`Set` returns its value; `SetDelayed` and `Clear` return `Null`.

### 3.11 Evaluation (core/eval.h)

Arguments are evaluated unless the head holds them (`HoldFirst`, `HoldRest`, `HoldAll`);
`Listable` heads (Plus, Times, Power, Sin, …) thread over lists of equal length;
then built-ins, user definitions (exact before pattern), and finally registered
`MathBackend`s are tried. Every result carries a status: `exact`, `verified`,
`numeric`, or `unverified` (see AGENTS.md, rules for backends).

### 3.12 Operator precedence (plain text, high to low)

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1 | function call `f[x]`, `'` (derivative), postfix `!` (factorial) | left |
| 2 | `^` | right |
| 3 | unary `-`, `+`, prefix `!` (Not) | — |
| 4 | `.` (Dot) | left |
| 5 | `*`, `/`, implicit multiplication (`2x`, `a b`) | left |
| 6 | `+`, `-` | left |
| 7 | `==`, `!=`, `<`, `<=`, `>`, `>=` | — |
| 8 | `&&` | left |
| 9 | `\|\|` | left |
| 10 | `->` | right |
| 11 | `/.` | left |
| 12 | `=`, `:=` | right |
| 13 | `;` | — |

A `.` between digits is a decimal point (`1.5`); between other operands it is Dot (`A . B`).

## 4. Editor ⇄ engine exchange format

- The app's math editor (MathLive) produces **MathJSON**. `convert/` maps MathJSON
  heads to the heads above (e.g. MathJSON `"Add"` → `Plus`, `"Multiply"` → `Times`,
  `"Integrate"` → `Integrate`, `"Matrix"` → `List` of rows).
- The plain-text parser produces the same `Expr` tree.
- Switching a cell between 2-D and text = `Expr` → printer for the other mode.
- The engine returns results as `Expr`; `convert/` renders them to MathJSON/LaTeX
  (2-D display) or plain text.

## 5. Change process

Changing a node name or argument order requires a PR that updates this file,
`core/`, and `convert/` together, approved by the maintainer.
