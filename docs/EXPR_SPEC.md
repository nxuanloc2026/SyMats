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
| `Normal`   | a head applied to arguments          | `Plus(x, 1)`, `Integrate(f, x)` |

Later kinds (not in v0.1): `Real` (MPFR arbitrary precision), `String`.

Rules:
- Expressions are **immutable** and shared (`std::shared_ptr<const Expr>`).
- A `Normal` has a head (any Expr, usually a Symbol) and an ordered list of arguments.
- The **internal form** below (`Head(arg1, arg2, …)`) is also valid plain-text input.

## 2. Canonical form (engine guarantees)

The engine always returns expressions in canonical form, so equal math has equal trees.

| Rule | Example |
|------|---------|
| `Plus`/`Times` are flattened | `Plus(a, Plus(b, c))` → `Plus(a, b, c)` |
| Numbers are combined | `Plus(2, x, 3)` → `Plus(5, x)` |
| Like terms are collected | `x + x` → `Times(2, x)`; `x*x` → `Power(x, 2)` |
| Identities removed | `Plus(x, 0)` → `x`; `Times(1, x)` → `x`; `Times(0, x)` → `0` |
| Single argument unwrapped | `Plus(x)` → `x` |
| Arguments sorted (canonical order) | `Plus(y, x, 1)` → `Plus(1, x, y)` |
| Subtraction / division are not nodes | `a - b` = `Plus(a, Times(-1, b))`; `a / b` = `Times(a, Power(b, -1))` |
| Rational powers of numbers stay exact | `Power(4, 1/2)` → `2`; `Power(2, 1/2)` stays |

Canonical order: numbers first (by value), then symbols (alphabetical), then normals
(by head, then arguments). Powers of the same base sort next to the base.

## 3. Node names

Notation columns: **Text** = plain-text input/output; **2-D** = what the math editor shows.
Text names are lowercase aliases; the internal form uses the capitalized head.

### 3.1 Arithmetic

| Head | Arguments | Text | 2-D |
|------|-----------|------|-----|
| `Plus` | terms… | `a + b` | a + b |
| `Times` | factors… | `a*b`, `a b` | a·b |
| `Power` | base, exponent | `a^b` | aᵇ |
| `Sqrt` (input only → `Power(x, 1/2)`) | x | `sqrt(x)` | √x |
| `Root` (input only → `Power(x, 1/n)`) | x, n | `root(x, n)` | ⁿ√x |
| `Abs` | x | `abs(x)` | \|x\| |
| `Factorial` | n | `n!` | n! |

### 3.2 Constants (Symbols)

`Pi` (π), `E` (e), `I` (i), `Infinity` (∞), `True`, `False`.
Text: `pi`, `e`, `i`, `inf`/`infinity`.

### 3.3 Functions

`Sin Cos Tan Cot Sec Csc ArcSin ArcCos ArcTan Sinh Cosh Tanh Exp Log`
Text: lowercase (`sin(x)`, `arctan(x)`, `log(x)` = natural log, `log(b, x)` = base b).

### 3.4 Relations and logic

| Head | Text | 2-D |
|------|------|-----|
| `Equal` | `a == b` (also `a = b` inside solve/dsolve) | a = b |
| `Unequal` | `a != b` | a ≠ b |
| `Less`, `LessEqual`, `Greater`, `GreaterEqual` | `<  <=  >  >=` | <  ≤  >  ≥ |
| `And`, `Or`, `Not` | `and or not` | ∧ ∨ ¬ |
| `List` | `{a, b, c}` | {a, b, c} |
| `Rule` | `x -> 2` | x → 2 |

### 3.5 Calculus

| Head | Arguments | Text | 2-D |
|------|-----------|------|-----|
| `D` | f, x | `D(f, x)` / `diff(f, x)` | d/dx f |
| `D` | f, {x, n} | `D(f, x, n)` | dⁿ/dxⁿ f |
| `D` | f, x, y | `D(f, x, y)` | ∂²f/∂x∂y |
| `Derivative` | n, f (function form) | `y'`, `y''` | y′, y″ |
| `Integrate` | f, x | `integrate(f, x)` | ∫ f dx |
| `Integrate` | f, {x, a, b} | `integrate(f, x, a, b)` | ∫ₐᵇ f dx |
| `Integrate` | f, {x, a, b}, {y, c, d} | `integrate(f, x, a, b, y, c, d)` | ∬ f dx dy (limits may depend on outer variables) |
| `Limit` | f, Rule(x, a) | `limit(f, x -> a)` | lim_{x→a} f |
| `Sum` | f, {k, a, b} | `sum(f, k, a, b)` | Σ |
| `Product` | f, {k, a, b} | `product(f, k, a, b)` | Π |
| `Series` | f, {x, a, n} | `series(f, x, a, n)` | Taylor series |

### 3.6 Differential equations

| Head | Meaning | Text example |
|------|---------|--------------|
| `DSolve` | symbolic solve | `dsolve(y'' + y == 0, y(x), x)` |
| `DSolve` (system) | eqns list, funcs list, var | `dsolve({x'(t) == y(t), y'(t) == -x(t)}, {x(t), y(t)}, t)` |
| `DSolve` (PDE) | eqn, u(x,t), {x, t} | `dsolve(D(u(x,t),t) == k*D(u(x,t),x,2), u(x,t), {x,t})` |
| `NDSolve` | numeric solve | `ndsolve({y' == -y, y(0) == 1}, y, {x, 0, 5})` |
| `NIntegrate` | numeric integral | `nintegrate(exp(-x^2), x, 0, inf)` |

Results are `List` of `Rule`s: `{{y(x) -> C1*cos(x) + C2*sin(x)}}`.
Numeric results return `InterpolatingFunction(…)` usable in `Plot`.

### 3.7 Matrices and linear algebra

A matrix is a `List` of row `List`s; a vector is a `List`.

| Head | Text | 2-D |
|------|------|-----|
| (matrix) | `[[a, b], [c, d]]` or `{{a, b}, {c, d}}` | bracketed grid template |
| `Dot` | `A . B` | A·B |
| `Transpose` | `transpose(A)` | Aᵀ |
| `Det` | `det(A)` | \|A\| or det A |
| `Inverse` | `inverse(A)` | A⁻¹ |
| `Eigenvalues`, `Eigenvectors` | `eigenvalues(A)` | |
| `Rank`, `Trace`, `RowReduce`, `CharPoly`, `MatrixExp` | lowercase | |
| `LinearSolve` | `linsolve(A, b)` | |

### 3.8 Algebra

`Expand`, `Factor`, `Simplify`, `Together`, `Apart`, `Solve`, `Substitute`
Text: `expand(e)`, `factor(e)`, `simplify(e)`, `solve(eqn, x)`, `subs(e, x -> 2)`.

### 3.9 Plotting

| Head | Text example |
|------|--------------|
| `Plot` | `plot(sin(x), x, 0, 2*pi)`; multiple: `plot({sin(x), cos(x)}, x, 0, 2*pi)` |
| `ParametricPlot` | `paramplot({cos(3t), sin(2t)}, t, 0, 2*pi)` |
| `PolarPlot` | `polarplot(1 + cos(th), th, 0, 2*pi)` |
| `ImplicitPlot` | `implicitplot(x^2 + y^2 == 1, x, -2, 2, y, -2, 2)` |
| `Plot3D` | `plot3d(sin(x)*cos(y), x, -3, 3, y, -3, 3)` |
| `ContourPlot` | `contourplot(x^2 - y^2, x, -2, 2, y, -2, 2)` |
| `Animate` | `animate(plot(sin(x - t), x, 0, 2*pi), t, 0, 10)` |
| `Slider` | `plot(sin(a*x), x, 0, 2*pi, slider(a, 0, 5))` |

### 3.10 Assignment

| Head | Text | Meaning |
|------|------|---------|
| `Set` | `f = expr` | assign now |
| `SetDelayed` | `f(x_) := x^2` | define rule, evaluate on use |
| `Pattern` | `x_` | matches anything, binds `x` |

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
