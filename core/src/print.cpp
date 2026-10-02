// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Internal ("full form") printer. The user-facing text and LaTeX printers live in
// convert/ (see AGENTS.md lanes).
#include "symats/expr.h"

namespace symats {

namespace {
void write(const Expr& e, std::string& out) {
    switch (e.kind()) {
        case Expr::Kind::Integer: out += e.integer().to_string(); return;
        case Expr::Kind::Rational: out += e.rational().to_string(); return;
        case Expr::Kind::Symbol: out += e.name(); return;
        case Expr::Kind::Normal:
            write(*e.head(), out);
            out += '(';
            for (std::size_t i = 0; i < e.size(); ++i) {
                if (i) out += ", ";
                write(*e.arg(i), out);
            }
            out += ')';
            return;
    }
}
}  // namespace

std::string to_full_form(const ExprPtr& e) {
    std::string out;
    write(*e, out);
    return out;
}

}  // namespace symats
