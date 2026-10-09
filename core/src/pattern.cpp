// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Structural pattern matching with sequence patterns and backtracking.
// Semantics follow the usual term-rewriting conventions (see e.g. F. Baader and
// T. Nipkow, "Term Rewriting and All That", Cambridge University Press, 1998).
#include "symats/pattern.h"

#include <stdexcept>
#include <vector>

namespace symats {

namespace {

const ExprPtr& sym_sequence() {
    static const ExprPtr s = make_symbol("Sequence");
    return s;
}

struct SeqInfo {
    bool is_sequence = false;
    std::size_t min_len = 1;
    const Expr* blank = nullptr;  // the Blank* node (holds the optional head)
    std::string name;             // empty for anonymous
};

bool is_blank_kind(const Expr& e) {
    return e.has_head("Blank") || e.has_head("BlankSequence") || e.has_head("BlankNullSequence");
}

// Recognizes BlankSequence/BlankNullSequence, optionally wrapped in Pattern[name, ...].
SeqInfo sequence_info(const ExprPtr& p) {
    SeqInfo info;
    const Expr* b = p.get();
    if (p->has_head("Pattern") && p->size() == 2 && p->arg(0)->is_symbol()) {
        info.name = p->arg(0)->name();
        b = p->arg(1).get();
    }
    if (b->has_head("BlankSequence")) {
        info.is_sequence = true;
        info.min_len = 1;
    } else if (b->has_head("BlankNullSequence")) {
        info.is_sequence = true;
        info.min_len = 0;
    }
    info.blank = b;
    return info;
}

// Does `e` satisfy the head restriction of a Blank*(h) node?
bool head_ok(const Expr& blank_node, const Expr& e) {
    if (blank_node.size() == 0) return true;
    const ExprPtr& h = blank_node.arg(0);
    if (!h->is_symbol()) return false;
    return head_name(e) == h->name();
}

bool match_args(const ExprList& ps, std::size_t i, const ExprList& es, std::size_t j, Bindings& b);
bool match_flat_orderless(const ExprList& ps, std::size_t i, const ExprList& es,
                          std::vector<bool>& used, const ExprPtr& head, Bindings& b);

bool bind(const std::string& name, const ExprPtr& value, Bindings& b) {
    auto it = b.find(name);
    if (it != b.end()) return equal(it->second, value);
    b.emplace(name, value);
    return true;
}

}  // namespace

ExprPtr blank(std::string_view head) {
    if (head.empty()) return make_normal("Blank", {});
    return make_normal("Blank", {make_symbol(std::string(head))});
}

namespace {
ExprPtr named(std::string name, std::string_view kind, std::string_view head) {
    ExprList hargs;
    if (!head.empty()) hargs.push_back(make_symbol(std::string(head)));
    return make_normal("Pattern", {make_symbol(std::move(name)), make_normal(kind, std::move(hargs))});
}
}  // namespace

ExprPtr pat(std::string name, std::string_view head) { return named(std::move(name), "Blank", head); }
ExprPtr pat_seq(std::string name, std::string_view head) {
    return named(std::move(name), "BlankSequence", head);
}
ExprPtr pat_null_seq(std::string name, std::string_view head) {
    return named(std::move(name), "BlankNullSequence", head);
}

std::string head_name(const Expr& e) {
    switch (e.kind()) {
        case Expr::Kind::Integer: return "Integer";
        case Expr::Kind::Rational: return "Rational";
        case Expr::Kind::Symbol: return "Symbol";
        case Expr::Kind::Normal: return e.head()->is_symbol() ? e.head()->name() : std::string();
    }
    return {};
}

bool has_pattern(const ExprPtr& e) {
    if (!e->is_normal()) return false;
    if (is_blank_kind(*e) || e->has_head("Pattern")) return true;
    if (has_pattern(e->head())) return true;
    for (const auto& a : e->args())
        if (has_pattern(a)) return true;
    return false;
}

bool match(const ExprPtr& p, const ExprPtr& e, Bindings& bindings) {
    // Named pattern: Pattern[name, sub].
    if (p->has_head("Pattern") && p->size() == 2 && p->arg(0)->is_symbol()) {
        Bindings trial = bindings;
        const ExprPtr& sub = p->arg(1);
        ExprPtr value = e;
        if (sub->has_head("BlankSequence") || sub->has_head("BlankNullSequence")) {
            // Outside an argument list a sequence pattern matches exactly one expression.
            if (!head_ok(*sub, *e)) return false;
            value = make_normal(sym_sequence(), {e});
        } else if (!match(sub, e, trial)) {
            return false;
        }
        if (!bind(p->arg(0)->name(), value, trial)) return false;
        bindings = std::move(trial);
        return true;
    }
    if (is_blank_kind(*p)) return head_ok(*p, *e);

    if (!p->is_normal()) return equal(p, e);
    if (!e->is_normal()) return false;

    Bindings trial = bindings;
    if (!match(p->head(), e->head(), trial)) return false;
    const bool flat_orderless = p->head()->is_symbol() &&
                               (p->head()->is_symbol("Plus") || p->head()->is_symbol("Times"));
    if (flat_orderless) {
        std::vector<bool> used(e->size(), false);
        if (!match_flat_orderless(p->args(), 0, e->args(), used, p->head(), trial)) return false;
    } else if (!match_args(p->args(), 0, e->args(), 0, trial)) {
        return false;
    }
    bindings = std::move(trial);
    return true;
}

namespace {

ExprPtr grouped(const ExprPtr& head, const ExprList& terms) {
    if (terms.size() == 1) return terms.front();
    return make_normal(head, terms);
}

bool all_head_ok(const Expr& blank_node, const ExprList& terms) {
    for (const auto& term : terms)
        if (!head_ok(blank_node, *term)) return false;
    return true;
}

bool choose_flat_terms(const ExprList& es, std::size_t start, std::size_t count,
                       std::vector<bool>& used, ExprList& selected) {
    if (selected.size() == count) return true;
    for (std::size_t i = start; i < es.size(); ++i) {
        if (used[i]) continue;
        used[i] = true;
        selected.push_back(es[i]);
        if (choose_flat_terms(es, i + 1, count, used, selected)) return true;
        selected.pop_back();
        used[i] = false;
    }
    return false;
}

bool match_flat_orderless(const ExprList& ps, std::size_t i, const ExprList& es,
                          std::vector<bool>& used, const ExprPtr& head, Bindings& b) {
    if (i == ps.size()) {
        for (bool selected : used)
            if (!selected) return false;
        return true;
    }

    const SeqInfo info = sequence_info(ps[i]);
    std::size_t remaining = 0;
    for (bool selected : used)
        if (!selected) ++remaining;
    const std::size_t min_len = info.is_sequence ? info.min_len : 1;
    const std::size_t max_len = remaining;

    for (std::size_t len = min_len; len <= max_len; ++len) {
        ExprList selected;
        std::vector<bool> candidate_used = used;
        if (!choose_flat_terms(es, 0, len, candidate_used, selected)) continue;
        if (info.is_sequence && !all_head_ok(*info.blank, selected)) continue;

        ExprPtr value = grouped(head, selected);
        Bindings trial = b;
        bool matched = false;
        if (info.is_sequence) {
            if (!info.name.empty())
                matched = bind(info.name, make_normal(sym_sequence(), selected), trial);
            else
                matched = true;
        } else {
            matched = match(ps[i], value, trial);
        }
        if (!matched) continue;

        if (match_flat_orderless(ps, i + 1, es, candidate_used, head, trial)) {
            used = std::move(candidate_used);
            b = std::move(trial);
            return true;
        }
    }
    return false;
}

bool match_args(const ExprList& ps, std::size_t i, const ExprList& es, std::size_t j, Bindings& b) {
    if (i == ps.size()) return j == es.size();
    const SeqInfo info = sequence_info(ps[i]);

    if (!info.is_sequence) {
        if (j >= es.size()) return false;
        Bindings trial = b;
        if (match(ps[i], es[j], trial) && match_args(ps, i + 1, es, j + 1, trial)) {
            b = std::move(trial);
            return true;
        }
        return false;
    }

    // Sequence pattern: try the shortest run first (deterministic results).
    const std::size_t available = es.size() - j;
    for (std::size_t len = info.min_len; len <= available; ++len) {
        if (len > 0 && !head_ok(*info.blank, *es[j + len - 1])) break;  // longer runs fail too
        Bindings trial = b;
        if (!info.name.empty()) {
            ExprList run(es.begin() + static_cast<std::ptrdiff_t>(j),
                         es.begin() + static_cast<std::ptrdiff_t>(j + len));
            if (!bind(info.name, make_normal(sym_sequence(), std::move(run)), trial)) continue;
        }
        if (match_args(ps, i + 1, es, j + len, trial)) {
            b = std::move(trial);
            return true;
        }
    }
    return false;
}

}  // namespace

ExprPtr substitute(const ExprPtr& e, const Bindings& bindings) {
    if (bindings.empty()) return e;
    if (e->is_symbol()) {
        auto it = bindings.find(e->name());
        return it == bindings.end() ? e : it->second;
    }
    if (!e->is_normal()) return e;

    bool changed = false;
    ExprPtr head = substitute(e->head(), bindings);
    if (head != e->head()) changed = true;
    ExprList args;
    args.reserve(e->size());
    for (const auto& a : e->args()) {
        ExprPtr s = substitute(a, bindings);
        if (s != a) changed = true;
        if (s != a && s->has_head("Sequence")) {
            args.insert(args.end(), s->args().begin(), s->args().end());
        } else {
            args.push_back(std::move(s));
        }
    }
    if (!changed) return e;
    return make_normal(std::move(head), std::move(args));
}

ExprPtr replace_all(const ExprPtr& e, const ExprList& rules, bool* changed) {
    for (const auto& r : rules) {
        if (!(r->has_head("Rule") || r->has_head("RuleDelayed")) || r->size() != 2)
            throw std::invalid_argument("replace_all: expected Rule[lhs, rhs] or RuleDelayed[lhs, rhs]");
        Bindings b;
        if (match(r->arg(0), e, b)) {
            if (changed) *changed = true;
            return substitute(r->arg(1), b);
        }
    }
    if (!e->is_normal()) return e;

    bool any = false;
    ExprPtr head = replace_all(e->head(), rules, &any);
    ExprList args;
    args.reserve(e->size());
    for (const auto& a : e->args()) args.push_back(replace_all(a, rules, &any));
    if (!any) return e;
    if (changed) *changed = true;
    return make_normal(std::move(head), std::move(args));
}

}  // namespace symats
