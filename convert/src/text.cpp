// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/text.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace symats {
namespace {

std::string head_for(std::string_view name) {
    constexpr std::pair<std::string_view, std::string_view> aliases[] = {
        {"sin", "Sin"}, {"cos", "Cos"}, {"tan", "Tan"}, {"cot", "Cot"},
        {"sec", "Sec"}, {"csc", "Csc"}, {"arcsin", "ArcSin"},
        {"arccos", "ArcCos"}, {"arctan", "ArcTan"}, {"sinh", "Sinh"},
        {"cosh", "Cosh"}, {"tanh", "Tanh"}, {"exp", "Exp"}, {"log", "Log"},
        {"abs", "Abs"}, {"diff", "D"}, {"integrate", "Integrate"},
        {"nintegrate", "NIntegrate"}, {"dsolve", "DSolve"},
        {"ndsolve", "NDSolve"}, {"limit", "Limit"}, {"sum", "Sum"},
        {"product", "Product"}, {"series", "Series"}, {"transpose", "Transpose"},
        {"det", "Det"}, {"inverse", "Inverse"}, {"rank", "Rank"},
        {"trace", "Trace"}, {"eigenvalues", "Eigenvalues"},
        {"eigenvectors", "Eigenvectors"}, {"rref", "RowReduce"},
        {"charpoly", "CharPoly"}, {"matrixexp", "MatrixExp"},
        {"linsolve", "LinearSolve"}, {"expand", "Expand"}, {"factor", "Factor"},
        {"simplify", "Simplify"}, {"solve", "Solve"}, {"subs", "Substitute"},
        {"plot", "Plot"}, {"paramplot", "ParametricPlot"},
        {"polarplot", "PolarPlot"}, {"implicitplot", "ImplicitPlot"},
        {"plot3d", "Plot3D"}, {"contourplot", "ContourPlot"},
        {"animate", "Animate"}, {"slider", "Slider"},
    };
    for (const auto& [alias, head] : aliases) {
        if (name == alias) return std::string(head);
    }
    return std::string(name);
}

ExprPtr call(std::string_view name, ExprList args) {
    const std::string head = head_for(name);
    if (name == "ExprApply" && !args.empty()) {
        ExprPtr applied_head = args.front();
        args.erase(args.begin());
        return make_normal(std::move(applied_head), std::move(args));
    }
    if (head == "Plus") return plus(std::move(args));
    if (head == "Times") return times(std::move(args));
    if (head == "Power" && args.size() == 2) return power(args[0], args[1]);
    if (head == "List") return make_normal("List", std::move(args));
    if (name == "sqrt" && args.size() == 1)
        return power(args[0], make_rational(1, 2));
    if (name == "root" && args.size() == 2)
        return power(args[0], divide(make_integer(1), args[1]));
    if (name == "integrate" && args.size() >= 4 && (args.size() - 1) % 3 == 0) {
        ExprList grouped{args[0]};
        for (std::size_t i = 1; i < args.size(); i += 3)
            grouped.push_back(make_normal("List", {args[i], args[i + 1], args[i + 2]}));
        return make_normal("Integrate", std::move(grouped));
    }
    if ((name == "sum" || name == "product" || name == "series") && args.size() == 4)
        return make_normal(head, {args[0], make_normal("List", {args[1], args[2], args[3]})});
    if ((name == "D" || name == "diff") && args.size() == 3 && args[2]->is_integer())
        return make_normal("D", {args[0], make_normal("List", {args[1], args[2]})});
    return make_normal(head, std::move(args));
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    ExprPtr parse() {
        ExprPtr result = relation();
        space();
        if (pos_ != input_.size()) error("unexpected character");
        return result;
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;

    void space() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_])))
            ++pos_;
    }
    char peek() {
        space();
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }
    bool take(char c) {
        if (peek() != c) return false;
        ++pos_;
        return true;
    }
    [[noreturn]] void error(const char* what) const {
        throw std::invalid_argument(std::string("text expression at offset ") +
                                    std::to_string(pos_) + ": " + what);
    }
    void expect(char c) {
        if (!take(c)) error("missing delimiter");
    }
    bool take_token(std::string_view token) {
        space();
        if (input_.substr(pos_, token.size()) != token) return false;
        pos_ += token.size();
        return true;
    }
    ExprPtr relation() {
        ExprPtr left = sum();
        if (take_token("->")) return make_normal("Rule", {left, relation()});
        constexpr std::pair<std::string_view, std::string_view> operators[] = {
            {"==", "Equal"}, {"!=", "Unequal"}, {"<=", "LessEqual"},
            {">=", "GreaterEqual"}, {"<", "Less"}, {">", "Greater"}, {"=", "Equal"},
        };
        for (const auto& [token, head] : operators) {
            if (take_token(token)) return make_normal(head, {left, sum()});
        }
        return left;
    }
    ExprPtr sum() {
        ExprPtr left = product();
        while (true) {
            if (take('+')) left = plus(left, product());
            else if (peek() == '-' && input_.substr(pos_, 2) != "->" && take('-'))
                left = subtract(left, product());
            else return left;
        }
    }
    ExprPtr product() {
        ExprPtr left = unary();
        while (true) {
            if (take('*')) left = times(left, unary());
            else if (take('/')) left = divide(left, unary());
            else {
                // Juxtaposition, such as 2x or a b, denotes multiplication.
                char c = peek();
                if (c == '(' || c == '[' || std::isalpha(static_cast<unsigned char>(c)))
                    left = times(left, unary());
                else return left;
            }
        }
    }
    ExprPtr unary() {
        if (take('+')) return unary();
        if (take('-')) return negate(unary());
        return exponent();
    }
    ExprPtr exponent() {
        ExprPtr base = atom();
        if (take('^')) return power(base, unary());  // right associative
        return base;
    }
    ExprPtr atom() {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t start = pos_;
            while (pos_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
            return make_integer(Integer::from_string(input_.substr(start, pos_ - start)));
        }
        if (take('`')) {
            std::string name;
            while (pos_ < input_.size()) {
                char next = input_[pos_++];
                if (next == '`') {
                    if (pos_ < input_.size() && input_[pos_] == '`') {
                        name += '`';
                        ++pos_;
                    } else {
                        if (name.empty()) error("empty quoted symbol");
                        return make_symbol(std::move(name));
                    }
                } else name += next;
            }
            error("unterminated quoted symbol");
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_++;
            while (pos_ < input_.size() &&
                   (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
                    input_[pos_] == '_')) ++pos_;
            std::string name(input_.substr(start, pos_ - start));
            if (!take('(')) {
                if (name == "pi") name = "Pi";
                else if (name == "e") name = "E";
                else if (name == "i") name = "I";
                else if (name == "inf" || name == "infinity") name = "Infinity";
                return make_symbol(std::move(name));
            }
            ExprList args;
            if (!take(')')) {
                do { args.push_back(relation()); } while (take(','));
                expect(')');
            }
            return call(name, std::move(args));
        }
        if (take('(')) {
            ExprPtr inside = relation();
            expect(')');
            return inside;
        }
        if (c == '[' || c == '{') {
            ++pos_;
            char close = c == '[' ? ']' : '}';
            ExprList items;
            if (!take(close)) {
                do { items.push_back(relation()); } while (take(','));
                expect(close);
            }
            return make_normal("List", std::move(items));
        }
        error("expected expression");
    }
};

int precedence(const ExprPtr& e) {
    if (e->has_head("Plus")) return 10;
    if (e->has_head("Times")) return 20;
    if (e->has_head("Power")) return 40;
    return 50;
}

std::string write(const ExprPtr& e, int parent) {
    std::string out;
    int own = precedence(e);
    if (e->is_integer()) out = e->integer().to_string();
    else if (e->is_rational()) {
        out = e->rational().to_string();
        own = 20;
    } else if (e->is_symbol()) {
        const auto& name = e->name();
        if (name == "Pi") out = "pi";
        else if (name == "E") out = "e";
        else if (name == "I") out = "i";
        else if (name == "Infinity") out = "inf";
        else {
            bool bare = !name.empty() &&
                (std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_');
            for (char c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') bare = false;
            }
            if (name == "pi" || name == "e" || name == "i" ||
                name == "inf" || name == "infinity") bare = false;
            if (bare) out = name;
            else {
                out = "`";
                for (char c : name) {
                    out += c;
                    if (c == '`') out += '`';
                }
                out += "`";
            }
        }
    } else if (e->has_head("Plus") && e->size() > 0) {
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += " + ";
            out += write(e->arg(i), own);
        }
    } else if (e->has_head("Times") && e->size() > 0) {
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += "*";
            out += write(e->arg(i), own + 1);
        }
    } else if (e->has_head("Power") && e->size() == 2) {
        std::string base = write(e->arg(0), own + 1);
        if (e->arg(0)->is_integer() && e->arg(0)->integer().is_negative())
            base = "(" + base + ")";
        out = base + "^" + write(e->arg(1), own);
    } else if (e->has_head("List")) {
        out = "{";
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0);
        }
        out += "}";
    } else {
        bool ordinary_head = e->head()->is_symbol() &&
            write(e->head(), 0) == e->head()->name() &&
            head_for(e->head()->name()) == e->head()->name() &&
            e->head()->name() != "sqrt" && e->head()->name() != "root" &&
            e->head()->name() != "ExprApply";
        out = ordinary_head ? e->head()->name() : "ExprApply";
        out += "(";
        if (!ordinary_head) {
            out += write(e->head(), 0);
            if (e->size()) out += ", ";
        }
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0);
        }
        out += ")";
    }
    return own < parent ? "(" + out + ")" : out;
}
}  // namespace

ExprPtr parse_text(std::string_view input) { return Parser(input).parse(); }
std::string to_text(const ExprPtr& expr) {
    if (!expr) throw std::invalid_argument("to_text: null expression");
    return write(expr, 0);
}

}  // namespace symats
