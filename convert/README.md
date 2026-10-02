# Text conversion status

`parse_text` and `to_text` are the first part of the converter. They share the
expression nodes in [EXPR_SPEC.md](../docs/EXPR_SPEC.md) with the C++ core.

Supported input includes exact integers and fractions, arithmetic, powers,
juxtaposition multiplication, function calls, lists and matrix literals,
comparison operators, rules, and the documented lowercase function aliases.
The printer produces parseable text for expression trees, including quoted
symbols and arbitrary expression heads.

The remaining converter work is MathJSON input/output and a LaTeX printer.
The plain-text grammar also still needs derivative prime notation, factorials,
dot products, logical operators, and assignment syntax. The app's editor toggle
depends on MathJSON conversion.
