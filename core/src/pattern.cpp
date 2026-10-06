// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Structural pattern matching with sequence patterns and backtracking.
// Semantics follow the usual term-rewriting conventions (see e.g. F. Baader and
// T. Nipkow, "Term Rewriting and All That", Cambridge University Press, 1998).
#include "symats/pattern.h"

#include <functional>
#include <stdexcept>
#include <vector>

namespace symats {

namespace {

enum : unsigned {
    Flat = 1u << 3,
    Orderless = 1u << 4,
};

const ExprPtr& sym_sequence() {
    static const ExprPtr s = make_symbol("Sequence");
    return s;
}

ExprPtr combine_terms(const ExprPtr& head, const ExprList& terms) {
    if (head->is_symbol("Plus")) return plus(terms);
    if (head->is_symbol("Times")) return times(terms);
    return make_normal(head, terms);
}

bool is_flat_head(const ExprPtr& head, unsigned attrs) {
    if (attrs & Flat) return true;
    return head->is_symbol("Plus") || head->is_symbol("Times");
}

bool is_orderless_head(const ExprPtr& head, unsigned attrs) {
    if (attrs & Orderless) return true;
    return head->is_symbol("Plus") || head->is_symbol("Times");
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

bool match_args(const ExprList& ps, std::size_t i, const ExprList& es, std::size_t j, Bindings& b, unsigned attrs);
bool match_args_flat(const ExprPtr& head, const ExprList& ps, std::size_t p_idx,
                     const ExprList& es, std::size_t e_idx, Bindings& b, unsigned attrs);
bool match_flat_orderless_step(const ExprPtr& head, const ExprList& ps, std::size_t p_idx,
                               std::vector<bool>& used, std::size_t unused_count,
                               const ExprList& es, Bindings& b, bool allow_flat, unsigned attrs);

bool add_binding(const std::string& name, const ExprPtr& value, Bindings& b) {
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

bool match(const ExprPtr& p, const ExprPtr& e, Bindings& bindings, unsigned attributes) {
    // Named pattern: Pattern(name, sub).
    if (p->has_head("Pattern") && p->size() == 2 && p->arg(0)->is_symbol()) {
        Bindings trial = bindings;
        const ExprPtr& sub = p->arg(1);
        ExprPtr value = e;
        if (sub->has_head("BlankSequence") || sub->has_head("BlankNullSequence")) {
            // Outside an argument list a sequence pattern matches exactly one expression.
            if (!head_ok(*sub, *e)) return false;
            value = make_normal(sym_sequence(), {e});
        } else if (!match(sub, e, trial, attributes)) {
            return false;
        }
        if (!add_binding(p->arg(0)->name(), value, trial)) return false;
        bindings = std::move(trial);
        return true;
    }
    if (is_blank_kind(*p)) return head_ok(*p, *e);

    if (!p->is_normal()) return equal(p, e);
    if (!e->is_normal()) return false;

    Bindings trial = bindings;
    if (!match(p->head(), e->head(), trial, attributes)) return false;

    const bool flat = is_flat_head(p->head(), attributes);
    const bool orderless = is_orderless_head(p->head(), attributes);

    if (orderless) {
        std::vector<bool> used(e->size(), false);
        if (!match_flat_orderless_step(p->head(), p->args(), 0, used, e->size(), e->args(), trial, flat, attributes))
            return false;
    } else if (flat) {
        if (!match_args_flat(p->head(), p->args(), 0, e->args(), 0, trial, attributes))
            return false;
    } else {
        if (!match_args(p->args(), 0, e->args(), 0, trial, attributes))
            return false;
    }

    bindings = std::move(trial);
    return true;
}

namespace {

bool match_args(const ExprList& ps, std::size_t i, const ExprList& es, std::size_t j, Bindings& b, unsigned attrs) {
    if (i == ps.size()) return j == es.size();
    const SeqInfo info = sequence_info(ps[i]);

    if (!info.is_sequence) {
        if (j >= es.size()) return false;
        Bindings trial = b;
        if (match(ps[i], es[j], trial, attrs) && match_args(ps, i + 1, es, j + 1, trial, attrs)) {
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
            if (!add_binding(info.name, make_normal(sym_sequence(), std::move(run)), trial)) continue;
        }
        if (match_args(ps, i + 1, es, j + len, trial, attrs)) {
            b = std::move(trial);
            return true;
        }
    }
    return false;
}

bool match_args_flat(const ExprPtr& head, const ExprList& ps, std::size_t p_idx,
                     const ExprList& es, std::size_t e_idx, Bindings& b, unsigned attrs) {
    if (p_idx == ps.size()) return e_idx == es.size();

    const SeqInfo info = sequence_info(ps[p_idx]);
    const std::size_t available = es.size() - e_idx;

    if (p_idx == ps.size() - 1) {  // Last pattern parameter must consume all remaining elements
        const std::size_t len = available;
        if (info.is_sequence && info.min_len == 0 && len == 0) {
            Bindings trial = b;
            ExprPtr val = make_normal(sym_sequence(), {});
            if (!info.name.empty() && !add_binding(info.name, val, trial)) return false;
            b = std::move(trial);
            return true;
        }
        if (len < info.min_len) return false;
        ExprList slice(es.begin() + static_cast<std::ptrdiff_t>(e_idx), es.end());
        ExprPtr val;
        if (info.is_sequence) {
            val = make_normal(sym_sequence(), std::move(slice));
        } else if (slice.size() == 1) {
            val = slice[0];
        } else {
            val = combine_terms(head, slice);
        }

        Bindings trial = b;
        if (info.is_sequence) {
            if (!info.name.empty() && !add_binding(info.name, val, trial)) return false;
        } else {
            if (!match(ps[p_idx], val, trial, attrs)) return false;
        }
        b = std::move(trial);
        return true;
    }

    for (std::size_t len = info.min_len; len <= available; ++len) {
        if (len > 0 && info.is_sequence && !head_ok(*info.blank, *es[e_idx + len - 1])) break;
        ExprList slice(es.begin() + static_cast<std::ptrdiff_t>(e_idx),
                       es.begin() + static_cast<std::ptrdiff_t>(e_idx + len));
        ExprPtr val;
        if (info.is_sequence) {
            val = make_normal(sym_sequence(), std::move(slice));
        } else if (slice.size() == 1) {
            val = slice[0];
        } else {
            val = combine_terms(head, slice);
        }

        Bindings trial = b;
        bool ok = false;
        if (info.is_sequence) {
            ok = info.name.empty() || add_binding(info.name, val, trial);
        } else {
            ok = match(ps[p_idx], val, trial, attrs);
        }

        if (ok) {
            if (match_args_flat(head, ps, p_idx + 1, es, e_idx + len, trial, attrs)) {
                b = std::move(trial);
                return true;
            }
        }
    }
    return false;
}

bool match_flat_orderless_step(const ExprPtr& head, const ExprList& ps, std::size_t p_idx,
                               std::vector<bool>& used, std::size_t unused_count,
                               const ExprList& es, Bindings& b, bool allow_flat, unsigned attrs) {
    if (p_idx == ps.size()) return unused_count == 0;

    const SeqInfo info = sequence_info(ps[p_idx]);

    std::size_t rem_min = 0;
    for (std::size_t k = p_idx; k < ps.size(); ++k) {
        rem_min += sequence_info(ps[k]).min_len;
    }
    if (unused_count < rem_min) return false;

    auto make_value_for_subset = [&](const ExprList& subset) -> ExprPtr {
        if (info.is_sequence) {
            return make_normal(sym_sequence(), subset);
        }
        if (subset.size() == 1) return subset[0];
        return combine_terms(head, subset);
    };

    auto try_match_subset = [&](const ExprList& subset, const std::vector<std::size_t>& subset_indices) -> bool {
        for (std::size_t idx : subset_indices) used[idx] = true;
        ExprPtr val = make_value_for_subset(subset);
        Bindings trial = b;
        bool ok = false;
        if (info.is_sequence) {
            ok = info.name.empty() || add_binding(info.name, val, trial);
        } else {
            ok = match(ps[p_idx], val, trial, attrs);
        }
        if (ok) {
            if (match_flat_orderless_step(head, ps, p_idx + 1, used, unused_count - subset.size(), es, trial, allow_flat, attrs)) {
                b = std::move(trial);
                return true;
            }
        }
        for (std::size_t idx : subset_indices) used[idx] = false;
        return false;
    };

    if (p_idx == ps.size() - 1) {
        ExprList subset;
        std::vector<std::size_t> indices;
        subset.reserve(unused_count);
        indices.reserve(unused_count);
        for (std::size_t i = 0; i < es.size(); ++i) {
            if (!used[i]) {
                subset.push_back(es[i]);
                indices.push_back(i);
            }
        }
        if (!allow_flat && !info.is_sequence && subset.size() > 1) return false;
        if (subset.size() < info.min_len) return false;
        return try_match_subset(subset, indices);
    }

    if (info.is_sequence && info.min_len == 0) {
        ExprList empty_subset;
        std::vector<std::size_t> empty_indices;
        if (try_match_subset(empty_subset, empty_indices)) return true;
    }

    const std::size_t max_size = allow_flat || info.is_sequence ? unused_count - (rem_min - info.min_len) : 1;
    const std::size_t min_size = info.min_len;

    std::vector<std::size_t> unused_indices;
    unused_indices.reserve(unused_count);
    for (std::size_t i = 0; i < es.size(); ++i) {
        if (!used[i]) unused_indices.push_back(i);
    }

    for (std::size_t sz = min_size; sz <= max_size; ++sz) {
        if (sz == 0) continue;
        std::function<bool(std::size_t, std::size_t, std::vector<std::size_t>&)> gen_comb;
        gen_comb = [&](std::size_t start, std::size_t needed, std::vector<std::size_t>& current_indices) -> bool {
            if (needed == 0) {
                ExprList subset;
                subset.reserve(sz);
                for (std::size_t idx : current_indices) subset.push_back(es[idx]);
                return try_match_subset(subset, current_indices);
            }
            if (start > unused_indices.size() || unused_indices.size() - start < needed) return false;
            for (std::size_t i = start; i <= unused_indices.size() - needed; ++i) {
                current_indices.push_back(unused_indices[i]);
                if (gen_comb(i + 1, needed - 1, current_indices)) return true;
                current_indices.pop_back();
            }
            return false;
        };

        std::vector<std::size_t> current_indices;
        current_indices.reserve(sz);
        if (gen_comb(0, sz, current_indices)) return true;
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
            throw std::invalid_argument("replace_all: expected Rule(lhs, rhs) or RuleDelayed(lhs, rhs)");
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
