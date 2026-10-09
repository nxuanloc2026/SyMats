// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Term-rewriting evaluator. The overall design (held arguments, sequence splicing,
// listability, own/down values, rewrite-to-fixed-point) follows standard
// computer-algebra practice, e.g. R. Zippel, "Effective Polynomial Computation",
// and J. H. Davenport et al., "Computer Algebra: Systems and Algorithms for
// Algebraic Computation", Academic Press, 1988, ch. 1-2.
#include "symats/eval.h"
#include "symats/calculus.h"
#include "symats/numeric.h"
#include "symats/plot.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "symats/pattern.h"

namespace symats {

// ---------------------------------------------------------------- Context data

Context::SymbolData& Context::data(const std::string& symbol) { return symbols_[symbol]; }

const Context::SymbolData* Context::find(const std::string& symbol) const {
    auto it = symbols_.find(symbol);
    return it == symbols_.end() ? nullptr : &it->second;
}

void Context::set_value(const std::string& symbol, ExprPtr value) {
    if (attributes(symbol) & attr::Protected)
        throw EvaluationError("cannot assign to protected symbol " + symbol);
    data(symbol).own_value = std::move(value);
}

ExprPtr Context::value(const std::string& symbol) const {
    const SymbolData* d = find(symbol);
    return d ? d->own_value : nullptr;
}

void Context::add_definition(const std::string& symbol, ExprPtr lhs, ExprPtr rhs) {
    if (attributes(symbol) & attr::Protected)
        throw EvaluationError("cannot define rules for protected symbol " + symbol);
    SymbolData& d = data(symbol);
    auto& list = has_pattern(lhs) ? d.patterns : d.exact;
    for (auto& def : list) {
        if (equal(def.lhs, lhs)) {
            def.rhs = std::move(rhs);
            return;
        }
    }
    list.push_back({std::move(lhs), std::move(rhs)});
}

void Context::clear(const std::string& symbol) {
    if (attributes(symbol) & attr::Protected)
        throw EvaluationError("cannot clear protected symbol " + symbol);
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) return;
    it->second.own_value.reset();
    it->second.exact.clear();
    it->second.patterns.clear();
}

void Context::set_attributes(const std::string& symbol, unsigned attributes) {
    data(symbol).attributes = attributes;
}

unsigned Context::attributes(const std::string& symbol) const {
    const SymbolData* d = find(symbol);
    return d ? d->attributes : 0u;
}

void Context::set_builtin(const std::string& head, Builtin fn) { builtins_[head] = std::move(fn); }

std::vector<std::string> Context::user_symbols() const {
    std::vector<std::string> names;
    for (const auto& [name, d] : symbols_) {
        if (d.attributes & attr::Protected) continue;
        if (d.own_value || !d.exact.empty() || !d.patterns.empty()) names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// ---------------------------------------------------------------- helpers

namespace {

const ExprPtr& sym_null() {
    static const ExprPtr s = make_symbol("Null");
    return s;
}
const ExprPtr& sym_true() {
    static const ExprPtr s = make_symbol("True");
    return s;
}
const ExprPtr& sym_false() {
    static const ExprPtr s = make_symbol("False");
    return s;
}
const ExprPtr& sym_list() {
    static const ExprPtr s = make_symbol("List");
    return s;
}

struct DepthGuard {
    explicit DepthGuard(std::size_t& d, std::size_t limit) : depth(d) {
        if (++depth > limit) {
            --depth;
            throw EvaluationError("recursion depth limit exceeded");
        }
    }
    ~DepthGuard() { --depth; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    std::size_t& depth;
};

// The symbol a definition is attached to: f for f[...], x for x.
const std::string* definition_symbol(const ExprPtr& lhs) {
    if (lhs->is_symbol()) return &lhs->name();
    if (lhs->is_normal() && lhs->head()->is_symbol()) return &lhs->head()->name();
    return nullptr;
}

// Evaluate the arguments (not the head) of a definition's left-hand side, so that
// f[1 + 1] := ... defines f[2]. Pattern nodes are HoldFirst and stay intact.
ExprPtr evaluate_lhs(const ExprPtr& lhs, Context& ctx) {
    if (!lhs->is_normal()) return lhs;
    ExprList args;
    args.reserve(lhs->size());
    for (const auto& a : lhs->args()) args.push_back(evaluate(a, ctx));
    return make_normal(lhs->head(), std::move(args));
}

ExprPtr define(const ExprPtr& lhs_in, const ExprPtr& rhs, Context& ctx) {
    const std::string* s = definition_symbol(lhs_in);
    if (!s) throw EvaluationError("cannot assign to " + to_full_form(lhs_in));
    if (lhs_in->is_symbol()) {
        ctx.set_value(*s, rhs);
    } else {
        ctx.add_definition(*s, evaluate_lhs(lhs_in, ctx), rhs);
    }
    return rhs;
}

ExprList rules_from(const ExprPtr& r) {
    if (r->has_head("List")) return r->args();
    return {r};
}

// Truth value of a comparison between two exact numbers, if both are numbers.
int numeric_compare(const ExprPtr& a, const ExprPtr& b, bool& ok) {
    ok = a->is_number() && b->is_number();
    if (!ok) return 0;
    auto c = a->number() <=> b->number();
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

ExprPtr truth(bool v) { return v ? sym_true() : sym_false(); }

// ---- linear algebra helpers for Dot
// A vector is a non-empty List with no List elements; a matrix is a non-empty List of
// vectors of equal length.
bool is_vector(const ExprPtr& e) {
    if (!e->has_head("List") || e->size() == 0) return false;
    for (const auto& a : e->args())
        if (a->has_head("List")) return false;
    return true;
}
bool is_matrix(const ExprPtr& e) {
    if (!e->has_head("List") || e->size() == 0) return false;
    const std::size_t cols = e->arg(0)->size();
    for (const auto& r : e->args())
        if (!is_vector(r) || r->size() != cols) return false;
    return true;
}
ExprPtr inner(const ExprList& a, const ExprList& b) {
    ExprList terms;
    terms.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) terms.push_back(times(a[i], b[i]));
    return plus(std::move(terms));
}
ExprList column(const ExprPtr& m, std::size_t j) {
    ExprList c;
    c.reserve(m->size());
    for (const auto& row : m->args()) c.push_back(row->arg(j));
    return c;
}
[[noreturn]] void dot_mismatch(std::size_t a, std::size_t b) {
    throw EvaluationError("Dot: incompatible dimensions (" + std::to_string(a) + " and " +
                          std::to_string(b) + ")");
}
// Returns nullptr when the operands are not concrete vectors/matrices.
ExprPtr dot2(const ExprPtr& a, const ExprPtr& b) {
    const ExprPtr list = make_symbol("List");
    if (is_vector(a) && is_vector(b)) {
        if (a->size() != b->size()) dot_mismatch(a->size(), b->size());
        return inner(a->args(), b->args());
    }
    if (is_matrix(a) && is_vector(b)) {  // (m x n) . n -> m
        if (a->arg(0)->size() != b->size()) dot_mismatch(a->arg(0)->size(), b->size());
        ExprList out;
        for (const auto& row : a->args()) out.push_back(inner(row->args(), b->args()));
        return make_normal(list, std::move(out));
    }
    if (is_vector(a) && is_matrix(b)) {  // m . (m x n) -> n
        if (a->size() != b->size()) dot_mismatch(a->size(), b->size());
        ExprList out;
        for (std::size_t j = 0; j < b->arg(0)->size(); ++j) out.push_back(inner(a->args(), column(b, j)));
        return make_normal(list, std::move(out));
    }
    if (is_matrix(a) && is_matrix(b)) {  // (m x k) . (k x n) -> m x n
        if (a->arg(0)->size() != b->size()) dot_mismatch(a->arg(0)->size(), b->size());
        ExprList rows;
        for (const auto& row : a->args()) {
            ExprList r;
            for (std::size_t j = 0; j < b->arg(0)->size(); ++j) r.push_back(inner(row->args(), column(b, j)));
            rows.push_back(make_normal(list, std::move(r)));
        }
        return make_normal(list, std::move(rows));
    }
    return nullptr;
}

void install_builtins(Context& ctx) {
    using attr::Flat;
    using attr::HoldAll;
    using attr::HoldFirst;
    using attr::HoldRest;
    using attr::Listable;
    using attr::Orderless;
    using attr::Protected;

    // Arithmetic: canonical constructors do the work.
    ctx.set_builtin("Plus", [](const ExprPtr& e, Context&) { return plus(e->args()); });
    ctx.set_builtin("Times", [](const ExprPtr& e, Context&) { return times(e->args()); });
    ctx.set_builtin("Power", [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        return power(e->arg(0), e->arg(1));
    });
    ctx.set_attributes("Plus", Flat | Orderless | Listable | Protected);
    ctx.set_attributes("Times", Flat | Orderless | Listable | Protected);
    ctx.set_attributes("Power", Listable | Protected);

    // D holds the variable and its input until a local variable is known. This
    // prevents an own value of x from turning D[f[x], x] into D[f[3], 3].
    ctx.set_builtin("D", [](const ExprPtr& e, Context& c) -> ExprPtr {
        if (e->size() < 2) return nullptr;
        ExprPtr result = e->arg(0);
        bool can_evaluate_input = true;
        for (std::size_t i = 1; i < e->size(); ++i) {
            const ExprPtr& spec = e->arg(i);
            const ExprPtr& variable = spec->has_head("List") && spec->size() == 2
                ? spec->arg(0) : spec;
            if (!variable->is_symbol()) return nullptr;
            if (c.value(variable->name())) can_evaluate_input = false;
        }
        if (can_evaluate_input) result = evaluate(result, c);
        for (std::size_t i = 1; i < e->size(); ++i) {
            const ExprPtr& spec = e->arg(i);
            ExprPtr variable = spec;
            long long order = 1;
            if (spec->has_head("List") && spec->size() == 2) {
                variable = spec->arg(0);
                if (!spec->arg(1)->is_integer()) return nullptr;
                const auto n = spec->arg(1)->integer().to_int64();
                if (!n || *n < 0 || *n > 64) return nullptr;
                order = *n;
            }
            for (long long j = 0; j < order; ++j)
                result = differentiate(result, variable);
        }
        return result;
    });
    ctx.set_attributes("D", HoldAll | Protected);

    // Plots (symats/plot.h). Each also accepts a trailing Slider[a, lo, hi].
    const std::pair<const char*, ExprPtr (*)(const ExprPtr&, Context&)> plots[] = {
        {"Plot", plot},        {"ParametricPlot", parametric_plot}, {"PolarPlot", polar_plot},
        {"Plot3D", plot3d},    {"ContourPlot", contour_plot},       {"ImplicitPlot", implicit_plot},
    };
    for (const auto& [name, fn] : plots) {
        ctx.set_builtin(name, [fn](const ExprPtr& e, Context& c) {
            if (ExprPtr s = with_slider(e, c)) return s;
            return fn(e, c);
        });
        ctx.set_attributes(name, HoldAll | Protected);
    }
    ctx.set_builtin("Animate", [](const ExprPtr& e, Context& c) { return animate(e, c); });
    ctx.set_attributes("Animate", HoldAll | Protected);
    ctx.set_builtin("Expand", [](const ExprPtr& e, Context&) -> ExprPtr {
        return e->size() == 1 ? expand(e->arg(0)) : nullptr;
    });
    ctx.set_attributes("Expand", Protected);

    // Assignment.
    ctx.set_builtin("Set", [](const ExprPtr& e, Context& c) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        return define(e->arg(0), e->arg(1), c);
    });
    ctx.set_builtin("SetDelayed", [](const ExprPtr& e, Context& c) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        define(e->arg(0), e->arg(1), c);
        return sym_null();
    });
    ctx.set_builtin("Clear", [](const ExprPtr& e, Context& c) -> ExprPtr {
        for (const auto& a : e->args()) {
            if (!a->is_symbol()) throw EvaluationError("Clear expects symbols");
            c.clear(a->name());
        }
        return sym_null();
    });
    // a; b; c — evaluate in order, return the last.
    ctx.set_builtin("CompoundExpression", [](const ExprPtr& e, Context& c) -> ExprPtr {
        ExprPtr last = sym_null();
        for (const auto& a : e->args()) last = evaluate(a, c);
        return last;
    });
    ctx.set_attributes("CompoundExpression", HoldAll | Protected);
    ctx.set_attributes("Set", HoldFirst | Protected);
    ctx.set_attributes("SetDelayed", HoldAll | Protected);
    ctx.set_attributes("Clear", HoldAll | Protected);

    // Rules and replacement.
    auto substitute_builtin = [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        return replace_all(e->arg(0), rules_from(e->arg(1)));
    };
    ctx.set_builtin("Substitute", substitute_builtin);
    ctx.set_builtin("ReplaceAll", substitute_builtin);
    ctx.set_attributes("Substitute", Protected);
    ctx.set_attributes("ReplaceAll", Protected);
    ctx.set_attributes("Rule", Protected);
    ctx.set_attributes("RuleDelayed", HoldRest | Protected);

    // Patterns and holding.
    ctx.set_attributes("Pattern", HoldFirst | Protected);
    ctx.set_attributes("Blank", Protected);
    ctx.set_attributes("BlankSequence", Protected);
    ctx.set_attributes("BlankNullSequence", Protected);
    ctx.set_attributes("Hold", HoldAll | Protected);

    // Comparisons on exact numbers and identical expressions.
    ctx.set_builtin("Equal", [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        if (equal(e->arg(0), e->arg(1))) return sym_true();
        bool ok = false;
        int c = numeric_compare(e->arg(0), e->arg(1), ok);
        return ok ? truth(c == 0) : nullptr;
    });
    ctx.set_builtin("Unequal", [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() != 2) return nullptr;
        if (equal(e->arg(0), e->arg(1))) return sym_false();
        bool ok = false;
        int c = numeric_compare(e->arg(0), e->arg(1), ok);
        return ok ? truth(c != 0) : nullptr;
    });
    struct Cmp {
        const char* name;
        bool (*test)(int);
    };
    static const Cmp cmps[] = {
        {"Less", [](int c) { return c < 0; }},
        {"LessEqual", [](int c) { return c <= 0; }},
        {"Greater", [](int c) { return c > 0; }},
        {"GreaterEqual", [](int c) { return c >= 0; }},
    };
    for (const auto& cmp : cmps) {
        auto test = cmp.test;
        ctx.set_builtin(cmp.name, [test](const ExprPtr& e, Context&) -> ExprPtr {
            if (e->size() != 2) return nullptr;
            bool ok = false;
            int c = numeric_compare(e->arg(0), e->arg(1), ok);
            return ok ? truth(test(c)) : nullptr;
        });
        ctx.set_attributes(cmp.name, Protected);
    }
    ctx.set_attributes("Equal", Protected);
    ctx.set_attributes("Unequal", Protected);

    // Logic: And/Or evaluate left to right and stop early (HoldAll); undecided parts stay.
    ctx.set_builtin("And", [](const ExprPtr& e, Context& c) -> ExprPtr {
        ExprList rest;
        for (const auto& a : e->args()) {
            ExprPtr v = evaluate(a, c);
            if (v->is_symbol("False")) return sym_false();
            if (!v->is_symbol("True")) rest.push_back(v);
        }
        if (rest.empty()) return sym_true();
        if (rest.size() == 1) return rest[0];
        return make_normal("And", std::move(rest));
    });
    ctx.set_builtin("Or", [](const ExprPtr& e, Context& c) -> ExprPtr {
        ExprList rest;
        for (const auto& a : e->args()) {
            ExprPtr v = evaluate(a, c);
            if (v->is_symbol("True")) return sym_true();
            if (!v->is_symbol("False")) rest.push_back(v);
        }
        if (rest.empty()) return sym_false();
        if (rest.size() == 1) return rest[0];
        return make_normal("Or", std::move(rest));
    });
    ctx.set_builtin("Not", [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() != 1) return nullptr;
        const ExprPtr& a = e->arg(0);
        if (a->is_symbol("True")) return sym_false();
        if (a->is_symbol("False")) return sym_true();
        if (a->has_head("Not") && a->size() == 1) return a->arg(0);
        return nullptr;
    });
    ctx.set_attributes("And", HoldAll | Protected);
    ctx.set_attributes("Or", HoldAll | Protected);
    ctx.set_attributes("Not", Protected);

    // Dot: vector.vector -> scalar, matrix.vector, vector.matrix, matrix.matrix.
    ctx.set_builtin("Dot", [](const ExprPtr& e, Context&) -> ExprPtr {
        if (e->size() < 2) return e->size() == 1 ? e->arg(0) : nullptr;
        ExprPtr acc = e->arg(0);
        for (std::size_t i = 1; i < e->size(); ++i) {
            ExprPtr r = dot2(acc, e->arg(i));
            if (!r) return nullptr;  // not lists: leave symbolic, e.g. A . B
            acc = r;
        }
        return acc;
    });
    ctx.set_attributes("Dot", Protected);

    // Elementary functions thread over lists; numerical evaluation comes later.
    for (const char* f : {"Sin", "Cos", "Tan", "Cot", "Sec", "Csc", "ArcSin", "ArcCos", "ArcTan",
                          "Sinh", "Cosh", "Tanh", "Exp", "Log", "Abs"})
        ctx.set_attributes(f, Listable | Protected);

    // Constants and structural heads.
    for (const char* s : {"Pi", "E", "I", "Infinity", "ComplexInfinity", "Indeterminate", "True",
                          "False", "Null", "List", "Sequence"})
        ctx.set_attributes(s, ctx.attributes(s) | Protected);
}

}  // namespace

Context::Context() { install_builtins(*this); }

// ---------------------------------------------------------------- evaluation

// Evaluate Derivative[n][f][x] for a function defined by the user. A fresh
// symbol keeps the function's argument symbolic while its definition expands.
static ExprPtr evaluated_derivative_application(const ExprPtr& cur,
                                                const ExprPtr& head, Context& ctx) {
    if (!head->is_normal() || head->size() != 1 ||
        !head->head()->has_head("Derivative") || head->head()->size() != 1 ||
        cur->size() != 1 || !head->head()->arg(0)->is_integer() ||
        !head->arg(0)->is_symbol()) return nullptr;
    const auto order = head->head()->arg(0)->integer().to_int64();
    if (!order || *order <= 0 || *order > 32) return nullptr;
    std::string name = "$SymatsDerivativeDummy";
    while (ctx.value(name)) name += "$";
    ExprPtr variable = make_symbol(name);
    ExprPtr call = make_normal(head->arg(0), {variable});
    ExprPtr body = evaluate(call, ctx);
    if (equal(body, call)) return nullptr;
    for (long long i = 0; i < *order; ++i)
        body = differentiate(body, variable);
    return substitute(body, {{name, cur->arg(0)}});
}

// InterpolatingFunction[{a, b}, data][t] for a number t: the interpolated value.
static ExprPtr interpolated_value(const ExprPtr& cur, const ExprPtr& head) {
    if (!head->has_head("InterpolatingFunction") || cur->size() != 1 || !cur->arg(0)->is_number())
        return nullptr;
    try {
        const double v = (*numeric::interpolating_samples(head))(cur->arg(0)->number().to_double());
        return std::isfinite(v) ? numeric::decimal(v) : nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

struct EvalStep {
    // One rewrite step. Returns {expr, changed}. When changed is false, `expr` is fully
    // evaluated (arguments evaluated, no rule applies).
    static std::pair<ExprPtr, bool> step(const ExprPtr& e, Context& ctx) {
        if (e->is_number()) return {e, false};

        if (e->is_symbol()) {
            const ExprPtr v = ctx.value(e->name());
            if (v && !equal(v, e)) return {v, true};
            return {e, false};
        }

        // Normal expression.
        ExprPtr head = evaluate(e->head(), ctx);
        const std::string* hname = head->is_symbol() ? &head->name() : nullptr;
        const unsigned attrs = hname ? ctx.attributes(*hname) : 0u;

        bool args_changed = head != e->head();
        ExprList args;
        args.reserve(e->size());
        for (std::size_t i = 0; i < e->size(); ++i) {
            const ExprPtr& a = e->arg(i);
            const bool hold = (i == 0) ? (attrs & attr::HoldFirst) : (attrs & attr::HoldRest);
            ExprPtr v = hold ? a : evaluate(a, ctx);
            if (v != a) args_changed = true;
            if (!(attrs & attr::SequenceHold) && v->has_head("Sequence") &&
                !(hname && *hname == "Sequence")) {
                args.insert(args.end(), v->args().begin(), v->args().end());
                args_changed = true;
            } else {
                args.push_back(std::move(v));
            }
        }

        // Listable: thread over list arguments of equal length.
        if (attrs & attr::Listable) {
            std::size_t len = 0;
            bool has_list = false, consistent = true;
            for (const auto& a : args) {
                if (!a->has_head("List")) continue;
                if (!has_list) {
                    len = a->size();
                    has_list = true;
                } else if (a->size() != len) {
                    consistent = false;
                }
            }
            if (has_list && consistent) {
                ExprList items;
                items.reserve(len);
                for (std::size_t k = 0; k < len; ++k) {
                    ExprList call_args;
                    call_args.reserve(args.size());
                    for (const auto& a : args) call_args.push_back(a->has_head("List") ? a->arg(k) : a);
                    items.push_back(make_normal(head, std::move(call_args)));
                }
                return {make_normal(sym_list(), std::move(items)), true};
            }
        }

        const ExprPtr cur = args_changed ? make_normal(head, std::move(args)) : e;
        if (!hname) {
            if (ExprPtr d = evaluated_derivative_application(cur, head, ctx)) return {d, true};
            if (ExprPtr v = interpolated_value(cur, head)) return {v, true};
            return {cur, false};
        }

        // 1. Built-in.
        if (auto it = ctx.builtins_.find(*hname); it != ctx.builtins_.end()) {
            ExprPtr r = it->second(cur, ctx);
            if (r && !equal(r, cur)) return {r, true};
        }

        // 2. User definitions: exact first, then patterns in definition order.
        if (const Context::SymbolData* d = ctx.find(*hname)) {
            // A definition that rewrites an expression to itself (f[x_] := f[x]) is
            // treated as not applying, instead of looping.
            for (const auto& def : d->exact)
                if (equal(def.lhs, cur) && !equal(def.rhs, cur)) return {def.rhs, true};
            for (const auto& def : d->patterns) {
                Bindings b;
                if (!match(def.lhs, cur, b)) continue;
                ExprPtr r = substitute(def.rhs, b);
                if (!equal(r, cur)) return {r, true};
            }
        }

        // 3. Backends.
        if (!ctx.backends_.empty()) {
            if (auto r = ctx.backends_.try_evaluate(cur)) {
                ctx.note_status(r->status);
                if (!equal(r->value, cur)) return {r->value, true};
            }
        }

        // Unchanged rule-wise. If only the arguments changed, `cur` is already final:
        // its arguments are evaluated and no rule applied.
        return {cur, false};
    }
};

ExprPtr evaluate(const ExprPtr& e0, Context& ctx) {
    if (ctx.depth_ == 0) ctx.iterations_ = 0;  // fresh rewrite budget per outermost call
    DepthGuard guard(ctx.depth_, ctx.max_depth);
    ExprPtr e = e0;
    while (true) {
        auto [next, changed] = EvalStep::step(e, ctx);
        if (!changed) return next;
        if (++ctx.iterations_ > ctx.max_iterations)
            throw EvaluationError("iteration limit exceeded (non-terminating definitions?)");
        e = std::move(next);
    }
}

EvalResult evaluate_top(const ExprPtr& e, Context& ctx) {
    ctx.reset_status();
    ExprPtr v = evaluate(e, ctx);
    return {v, ctx.status()};
}

}  // namespace symats
