// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/latex.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace symats {
namespace {

std::string symbol_to_latex(std::string_view name) {
    static const std::unordered_map<std::string_view, std::string_view> greek = {
        {"alpha", "\\alpha"}, {"beta", "\\beta"}, {"gamma", "\\gamma"}, {"delta", "\\delta"},
        {"epsilon", "\\epsilon"}, {"zeta", "\\zeta"}, {"eta", "\\eta"}, {"theta", "\\theta"},
        {"iota", "\\iota"}, {"kappa", "\\kappa"}, {"lambda", "\\lambda"}, {"mu", "\\mu"},
        {"nu", "\\nu"}, {"xi", "\\xi"}, {"pi", "\\pi"}, {"rho", "\\rho"},
        {"sigma", "\\sigma"}, {"tau", "\\tau"}, {"upsilon", "\\upsilon"}, {"phi", "\\phi"},
        {"chi", "\\chi"}, {"psi", "\\psi"}, {"omega", "\\omega"},
        {"Alpha", "A"}, {"Beta", "B"}, {"Gamma", "\\Gamma"}, {"Delta", "\\Delta"},
        {"Theta", "\\Theta"}, {"Lambda", "\\Lambda"}, {"Xi", "\\Xi"}, {"Pi", "\\Pi"},
        {"Sigma", "\\Sigma"}, {"Upsilon", "\\Upsilon"}, {"Phi", "\\Phi"}, {"Psi", "\\Psi"},
        {"Omega", "\\Omega"},
    };
    auto it = greek.find(name);
    if (it != greek.end()) return std::string(it->second);
    if (name == "Infinity") return "\\infty";
    if (name == "ComplexI" || name == "I") return "i";
    if (name == "E") return "e";
    return std::string(name);
}

bool is_matrix(const ExprPtr& e) {
    if (!e || !e->has_head("List") || e->size() == 0) return false;
    std::size_t cols = 0;
    for (std::size_t i = 0; i < e->size(); ++i) {
        const auto& row = e->arg(i);
        if (!row || !row->has_head("List") || row->size() == 0) return false;
        if (i == 0) cols = row->size();
        else if (row->size() != cols) return false;
    }
    return true;
}

bool negative_term(const ExprPtr& e) {
    if (!e) return false;
    if (e->is_number()) return e->number().sign() < 0;
    return e->has_head("Times") && e->size() > 0 &&
           e->arg(0)->is_number() && e->arg(0)->number().sign() < 0;
}

int precedence(const ExprPtr& e) {
    if (!e) return 0;
    if (e->has_head("Equal") || e->has_head("Unequal") || e->has_head("Less") ||
        e->has_head("LessEqual") || e->has_head("Greater") || e->has_head("GreaterEqual") ||
        e->has_head("Rule") || e->has_head("Set") || e->has_head("SetDelayed")) return 5;
    if (e->has_head("Plus")) return 10;
    if (e->has_head("Times")) return 20;
    if (e->has_head("Power")) return 40;
    return 50;
}

std::string format_latex(const ExprPtr& e, int parent_prec) {
    if (!e) return "";

    if (e->is_integer()) {
        return e->integer().to_string();
    }
    if (e->is_rational()) {
        const auto& r = e->rational();
        if (r.sign() < 0) {
            Rational pos(-r.num(), r.den());
            return "-\\frac{" + pos.num().to_string() + "}{" + pos.den().to_string() + "}";
        }
        return "\\frac{" + r.num().to_string() + "}{" + r.den().to_string() + "}";
    }
    if (e->is_symbol()) {
        return symbol_to_latex(e->name());
    }

    int own_prec = precedence(e);

    if (e->has_head("Plus") && e->size() > 0) {
        std::string out;
        for (std::size_t i = 0; i < e->size(); ++i) {
            const bool neg = negative_term(e->arg(i));
            if (i > 0) {
                out += neg ? " - " : " + ";
            } else if (neg) {
                out += "-";
            }
            out += format_latex(neg ? negate(e->arg(i)) : e->arg(i), own_prec + 1);
        }
        return own_prec < parent_prec ? "\\left(" + out + "\\right)" : out;
    }

    if (e->has_head("Times") && e->size() > 0) {
        std::string out;
        if (negative_term(e)) {
            ExprPtr pos = negate(e);
            out = "-" + format_latex(pos, 30);
            return parent_prec > 20 ? "\\left(" + out + "\\right)" : out;
        }
        for (std::size_t i = 0; i < e->size(); ++i) {
            const auto& arg = e->arg(i);
            if (i > 0) {
                if (arg->is_number() || (i > 0 && e->arg(i - 1)->is_number())) {
                    out += " \\cdot ";
                } else {
                    out += " ";
                }
            }
            out += format_latex(arg, own_prec + 1);
        }
        return own_prec < parent_prec ? "\\left(" + out + "\\right)" : out;
    }

    if (e->has_head("Power") && e->size() == 2) {
        if (e->arg(1)->is_rational() && e->arg(1)->rational().num() == Integer(1) &&
            e->arg(1)->rational().den() == Integer(2)) {
            return "\\sqrt{" + format_latex(e->arg(0), 0) + "}";
        }
        std::string base = format_latex(e->arg(0), own_prec + 1);
        std::string exp = format_latex(e->arg(1), 0);
        return base + "^{" + exp + "}";
    }

    if (e->has_head("List")) {
        if (is_matrix(e)) {
            std::string out = "\\begin{pmatrix}";
            for (std::size_t r = 0; r < e->size(); ++r) {
                if (r > 0) out += " \\\\ ";
                const auto& row = e->arg(r);
                for (std::size_t c = 0; c < row->size(); ++c) {
                    if (c > 0) out += " & ";
                    out += format_latex(row->arg(c), 0);
                }
            }
            out += "\\end{pmatrix}";
            return out;
        }
        std::string out = "\\left\\{";
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i > 0) out += ", ";
            out += format_latex(e->arg(i), 0);
        }
        out += "\\right\\}";
        return out;
    }

    if (e->has_head("Integrate")) {
        if (e->size() == 2) {
            return "\\int " + format_latex(e->arg(0), 0) + " \\, d" + format_latex(e->arg(1), 0);
        }
        if (e->size() == 3 && e->arg(1)->has_head("List") && e->arg(1)->size() == 3) {
            const auto& spec = e->arg(1);
            return "\\int_{" + format_latex(spec->arg(1), 0) + "}^{" +
                   format_latex(spec->arg(2), 0) + "} " + format_latex(e->arg(0), 0) +
                   " \\, d" + format_latex(spec->arg(0), 0);
        }
    }

    // Standard math functions
    static const std::unordered_map<std::string_view, std::string_view> math_funcs = {
        {"Sin", "\\sin"}, {"Cos", "\\cos"}, {"Tan", "\\tan"},
        {"Sinh", "\\sinh"}, {"Cosh", "\\cosh"}, {"Tanh", "\\tanh"},
        {"ArcSin", "\\arcsin"}, {"ArcCos", "\\arccos"}, {"ArcTan", "\\arctan"},
        {"Log", "\\ln"}, {"Exp", "\\exp"}, {"Abs", "|"},
    };

    if (e->head()->is_symbol()) {
        std::string_view head_name = e->head()->name();
        auto it = math_funcs.find(head_name);
        if (it != math_funcs.end()) {
            if (head_name == "Abs" && e->size() == 1) {
                return "\\left|" + format_latex(e->arg(0), 0) + "\\right|";
            }
            if (e->size() == 1) {
                return std::string(it->second) + "\\left(" + format_latex(e->arg(0), 0) + "\\right)";
            }
        }

        static const std::unordered_map<std::string_view, std::string_view> infix_ops = {
            {"Equal", "="}, {"Unequal", "\\neq"}, {"Less", "<"},
            {"LessEqual", "\\le"}, {"Greater", ">"}, {"GreaterEqual", "\\ge"},
            {"Rule", "\\to"}, {"Set", "="}, {"SetDelayed", ":="},
        };
        auto op_it = infix_ops.find(head_name);
        if (op_it != infix_ops.end() && e->size() == 2) {
            std::string out = format_latex(e->arg(0), own_prec + 1) + " " +
                              std::string(op_it->second) + " " +
                              format_latex(e->arg(1), own_prec + 1);
            return own_prec < parent_prec ? "\\left(" + out + "\\right)" : out;
        }
    }

    // Generic call: Head(arg1, arg2)
    std::string head_latex = format_latex(e->head(), 50);
    if (e->head()->is_symbol() && std::isupper(static_cast<unsigned char>(e->head()->name()[0]))) {
        head_latex = "\\operatorname{" + e->head()->name() + "}";
    }
    std::string out = head_latex + "\\left(";
    for (std::size_t i = 0; i < e->size(); ++i) {
        if (i > 0) out += ", ";
        out += format_latex(e->arg(i), 0);
    }
    out += "\\right)";
    return out;
}

}  // namespace

std::string to_latex(const ExprPtr& expr) {
    if (!expr) throw std::invalid_argument("to_latex: null expression");
    return format_latex(expr, 0);
}

}  // namespace symats
