// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Structural pattern matching with sequence patterns and backtracking.
// Semantics follow the usual term-rewriting conventions (see e.g. F. Baader and
// T. Nipkow, "Term Rewriting and All That", Cambridge University Press, 1998).
#include "symats/pattern.h"
#include "symats/eval.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

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

// Recognizes BlankSequence/BlankNullSequence, optionally wrapped in Pattern(name, ...).
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

bool match_impl(const ExprPtr& p, const ExprPtr& e, Bindings& b,
                const AttributeLookup& attributes);
bool match_args(const ExprList& ps, std::size_t i, const ExprList& es,
                std::size_t j, Bindings& b, const AttributeLookup& attributes);
bool match_special(const ExprList& ps, std::size_t i, const ExprList& remaining,
                   const ExprPtr& head, bool flat, bool orderless, Bindings& b,
                   const AttributeLookup& attributes, std::size_t& attempts);

bool bind_name(const std::string& name, const ExprPtr& value, Bindings& b) {
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

namespace {
bool match_impl(const ExprPtr& p, const ExprPtr& e, Bindings& bindings,
                const AttributeLookup& attributes) {
    // Named pattern: Pattern(name, sub).
    if (p->has_head("Pattern") && p->size() == 2 && p->arg(0)->is_symbol()) {
        Bindings trial = bindings;
        const ExprPtr& sub = p->arg(1);
        ExprPtr value = e;
        if (sub->has_head("BlankSequence") || sub->has_head("BlankNullSequence")) {
            // Outside an argument list a sequence pattern matches exactly one expression.
            if (!head_ok(*sub, *e)) return false;
            value = make_normal(sym_sequence(), {e});
        } else if (!match_impl(sub, e, trial, attributes)) {
            return false;
        }
        if (!bind_name(p->arg(0)->name(), value, trial)) return false;
        bindings = std::move(trial);
        return true;
    }
    if (is_blank_kind(*p)) return head_ok(*p, *e);

    if (!p->is_normal()) return equal(p, e);
    if (!e->is_normal()) return false;

    Bindings trial = bindings;
    if (!match_impl(p->head(), e->head(), trial, attributes)) return false;
    unsigned flags = 0;
    if (attributes && e->head()->is_symbol()) flags = attributes(e->head()->name());
    if (flags & (attr::Flat | attr::Orderless)) {
        std::size_t attempts = 0;
        if (!match_special(p->args(), 0, e->args(), e->head(),
                           (flags & attr::Flat) != 0, (flags & attr::Orderless) != 0,
                           trial, attributes, attempts)) return false;
    } else if (!match_args(p->args(), 0, e->args(), 0, trial, attributes)) return false;
    bindings = std::move(trial);
    return true;
}
}  // namespace

bool match(const ExprPtr& p, const ExprPtr& e, Bindings& bindings) {
    return match_impl(p, e, bindings, {});
}

bool match_with_attributes(const ExprPtr& p, const ExprPtr& e,
                           Bindings& bindings, const AttributeLookup& attributes) {
    return match_impl(p, e, bindings, attributes);
}

namespace {

bool match_args(const ExprList& ps, std::size_t i, const ExprList& es, std::size_t j,
                Bindings& b, const AttributeLookup& attributes) {
    if (i == ps.size()) return j == es.size();
    const SeqInfo info = sequence_info(ps[i]);

    if (!info.is_sequence) {
        if (j >= es.size()) return false;
        Bindings trial = b;
        if (match_impl(ps[i], es[j], trial, attributes) &&
            match_args(ps, i + 1, es, j + 1, trial, attributes)) {
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
            if (!bind_name(info.name, make_normal(sym_sequence(), std::move(run)), trial)) continue;
        }
        if (match_args(ps, i + 1, es, j + len, trial, attributes)) {
            b = std::move(trial);
            return true;
        }
    }
    return false;
}

bool match_special(const ExprList& ps, std::size_t i, const ExprList& remaining,
                   const ExprPtr& head, bool flat, bool orderless, Bindings& b,
                   const AttributeLookup& attributes, std::size_t& attempts) {
    if (++attempts > 20000) return false;
    if (i == ps.size()) return remaining.empty();
    const SeqInfo info = sequence_info(ps[i]);
    std::size_t rest_min = 0;
    for (std::size_t j = i + 1; j < ps.size(); ++j) {
        const SeqInfo next = sequence_info(ps[j]);
        rest_min += next.is_sequence ? next.min_len : 1;
    }
    const std::size_t min_len = info.is_sequence ? info.min_len : 1;
    if (remaining.size() < min_len + rest_min) return false;
    const std::size_t max_len = std::min(
        info.is_sequence || flat ? remaining.size() : std::size_t(1),
        remaining.size() - rest_min);

    for (std::size_t len = min_len; len <= max_len; ++len) {
        const auto accept = [&](const ExprList& chosen, const ExprList& rest) {
            Bindings trial = b;
            if (info.is_sequence) {
                for (const auto& item : chosen)
                    if (!head_ok(*info.blank, *item)) return false;
                if (!info.name.empty() &&
                    !bind_name(info.name, make_normal(sym_sequence(), chosen), trial)) return false;
            } else {
                ExprPtr value = chosen.size() == 1 ? chosen.front()
                    : make_normal(head, chosen);
                if (!match_impl(ps[i], value, trial, attributes)) return false;
            }
            if (!match_special(ps, i + 1, rest, head, flat, orderless,
                               trial, attributes, attempts)) return false;
            b = std::move(trial);
            return true;
        };
        if (!orderless) {
            ExprList chosen(remaining.begin(), remaining.begin() + static_cast<std::ptrdiff_t>(len));
            ExprList rest(remaining.begin() + static_cast<std::ptrdiff_t>(len), remaining.end());
            if (accept(chosen, rest)) return true;
            continue;
        }
        std::vector<bool> picked(remaining.size(), false);
        const auto choose = [&](auto&& self, std::size_t start, std::size_t count) -> bool {
            if (++attempts > 20000) return false;
            if (count == len) {
                ExprList chosen, rest;
                for (std::size_t k = 0; k < remaining.size(); ++k)
                    (picked[k] ? chosen : rest).push_back(remaining[k]);
                return accept(chosen, rest);
            }
            for (std::size_t k = start; k + (len - count) <= remaining.size(); ++k) {
                picked[k] = true;
                if (self(self, k + 1, count + 1)) return true;
                picked[k] = false;
            }
            return false;
        };
        if (choose(choose, 0, 0)) return true;
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

namespace {
ExprPtr replace_impl(const ExprPtr& e, const ExprList& rules,
                     const AttributeLookup& attributes, bool* changed) {
    for (const auto& r : rules) {
        if (!(r->has_head("Rule") || r->has_head("RuleDelayed")) || r->size() != 2)
            throw std::invalid_argument("replace_all: expected Rule(lhs, rhs) or RuleDelayed(lhs, rhs)");
        Bindings b;
        if (match_impl(r->arg(0), e, b, attributes)) {
            if (changed) *changed = true;
            return substitute(r->arg(1), b);
        }
    }
    if (!e->is_normal()) return e;

    bool any = false;
    ExprPtr head = replace_impl(e->head(), rules, attributes, &any);
    ExprList args;
    args.reserve(e->size());
    for (const auto& a : e->args()) args.push_back(replace_impl(a, rules, attributes, &any));
    if (!any) return e;
    if (changed) *changed = true;
    return make_normal(std::move(head), std::move(args));
}
}  // namespace

ExprPtr replace_all(const ExprPtr& e, const ExprList& rules, bool* changed) {
    return replace_impl(e, rules, {}, changed);
}

ExprPtr replace_all_with_attributes(const ExprPtr& e, const ExprList& rules,
                                    const AttributeLookup& attributes, bool* changed) {
    return replace_impl(e, rules, attributes, changed);
}

}  // namespace symats
