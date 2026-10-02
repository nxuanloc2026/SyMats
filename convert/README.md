# Text conversion status

`parse_text` and `to_text` are the first part of the converter. They share the
expression nodes in [EXPR_SPEC.md](../docs/EXPR_SPEC.md) with the C++ core.

Supported input includes exact integers and fractions, arithmetic, powers,
juxtaposition multiplication, function calls, lists and matrix literals,
comparison operators, rules, and the documented lowercase function aliases.
The printer produces parseable text for expression trees, including quoted
symbols and arbitrary expression heads.

Decimals are exact fractions: `1.5` becomes `3/2`. A function call must have
the opening parenthesis immediately after its name: `f(x)` is a call, while
`f (x)` means multiplication. A single `=` is assignment (`Set`), `:=` is
delayed assignment (`SetDelayed`), and `==` is equality (`Equal`).
Recursive parsing and printing have a 256-call safety limit.

MathJSON input/output is exposed as `parse_mathjson` and `to_mathjson` in
`symats/mathjson.h`. It accepts serialized MathJSON numbers, symbols,
function arrays and object forms (`num`, `sym`, `fn`). The converter maps
`Add`/`Multiply` to canonical `Plus`/`Times`, MathLive `Tuple`/`Limits`
binders to `List`, and `Matrix` to a list of row lists. It handles MathLive's
structural and canonical integral forms. Integers and decimal/scientific
literals remain exact; unsupported MathJSON values such as strings, NaN and
repeating-decimal literals are rejected.

The remaining converter work is a LaTeX printer.
The plain-text grammar also still needs derivative prime notation, factorials,
dot products and logical operators. The app's editor toggle
depends on MathJSON conversion.
