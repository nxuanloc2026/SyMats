// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Pattern matching and rule replacement (docs/EXPR_SPEC.md §3.10).
//
// Pattern nodes:
//   Blank()                 `_`        matches any one expression
//   Blank(h)                `_h`       matches one expression whose head is h
//   BlankSequence(h?)       `__`       matches one or more arguments
//   BlankNullSequence(h?)   `___`      matches zero or more arguments
//   Pattern(name, blank)    `x_` ...   binds the match to `name`
//
// "Head" of an atom: Integer -> Integer, Rational -> Rational, Symbol -> Symbol.
// A sequence match is bound as Sequence(a1, a2, ...); Sequence is spliced into the
// argument list of the enclosing expression on substitution.
//
// Structural matching is available directly; attribute-aware matching uses
// the evaluator's Flat/Orderless flags (e.g. a_ + b_ on a three-term sum).
#pragma once

#include <functional>
#include <map>
#include <string>

#include "symats/expr.h"

namespace symats {

using Bindings = std::map<std::string, ExprPtr>;

// Helpers to build patterns in C++:  pat("x") is x_, pat("n", "Integer") is n_Integer,
// pat_seq("xs") is xs__, pat_null_seq("xs") is xs___.
ExprPtr blank(std::string_view head = {});
ExprPtr pat(std::string name, std::string_view head = {});
ExprPtr pat_seq(std::string name, std::string_view head = {});
ExprPtr pat_null_seq(std::string name, std::string_view head = {});

// The head name used by Blank(h) tests: "Integer", "Rational", "Symbol", or the
// head symbol's name for a Normal expression (empty if the head is not a symbol).
std::string head_name(const Expr& e);

// True if `e` contains any pattern node (Blank*, Pattern).
bool has_pattern(const ExprPtr& e);

// Try to match `expr` against `pattern`, extending `bindings`.
// On failure `bindings` is left unchanged. A name that is already bound must match
// a structurally equal value.
bool match(const ExprPtr& pattern, const ExprPtr& expr, Bindings& bindings);

// Match with the evaluator's Flat/Orderless attributes for each head.
using AttributeLookup = std::function<unsigned(std::string_view)>;
bool match_with_attributes(const ExprPtr& pattern, const ExprPtr& expr,
                           Bindings& bindings, const AttributeLookup& attributes);

// Replace every bound symbol in `e` by its value (Sequence values are spliced into
// argument lists). The result is rebuilt raw; evaluate it to canonicalize.
ExprPtr substitute(const ExprPtr& e, const Bindings& bindings);

// Apply the first matching rule at the top level; if none matches, recurse into the
// head and arguments. Each subexpression is rewritten at most once (one pass).
// `rules` holds Rule(lhs, rhs) / RuleDelayed(lhs, rhs) expressions.
// Sets *changed (if non-null) when anything was replaced.
ExprPtr replace_all(const ExprPtr& e, const ExprList& rules, bool* changed = nullptr);
ExprPtr replace_all_with_attributes(const ExprPtr& e, const ExprList& rules,
                                    const AttributeLookup& attributes,
                                    bool* changed = nullptr);

}  // namespace symats
