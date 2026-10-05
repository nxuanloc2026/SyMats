// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/text.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace symats {
namespace {

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
        {"clear", "Clear"}, {"plot", "Plot"}, {"paramplot", "ParametricPlot"},
        {"polarplot", "PolarPlot"}, {"implicitplot", "ImplicitPlot"},
        {"plot3d", "Plot3D"}, {"contourplot", "ContourPlot"},
        {"animate", "Animate"}, {"slider", "Slider"},
};

std::string head_for(std::string_view name) {
    for (const auto& [alias, head] : aliases) {
        if (name == alias) return std::string(head);
    }
    return std::string(name);
}

std::string text_for_head(std::string_view head) {
    return std::string(head);
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
    if ((head == "Sqrt" || name == "sqrt") && args.size() == 1)
        return power(args[0], make_rational(1, 2));
    if ((head == "Root" || name == "root") && args.size() == 2)
        return power(args[0], divide(make_integer(1), args[1]));
    if ((head == "Integrate" || name == "integrate") && args.size() >= 4 && (args.size() - 1) % 3 == 0) {
        ExprList grouped{args[0]};
        for (std::size_t i = 1; i < args.size(); i += 3)
            grouped.push_back(make_normal("List", {args[i], args[i + 1], args[i + 2]}));
        return make_normal("Integrate", std::move(grouped));
    }
    if ((head == "Sum" || head == "Product" || head == "Series" ||
         name == "sum" || name == "product" || name == "series") && args.size() == 4)
        return make_normal(head, {args[0], make_normal("List", {args[1], args[2], args[3]})});
    if ((head == "D" || name == "diff") && args.size() == 3 && args[2]->is_integer())
        return make_normal("D", {args[0], make_normal("List", {args[1], args[2]})});
    return make_normal(head, std::move(args));
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    ExprPtr parse() {
        ExprPtr result = statement();
        space();
        if (pos_ != input_.size()) error("unexpected character");
        return result;
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;
    std::size_t depth_ = 0;
    static constexpr std::size_t kMaxDepth = 256;

    struct DepthGuard {
        explicit DepthGuard(std::size_t& depth) : depth_(depth) {
            if (++depth_ > kMaxDepth) {
                --depth_;
                throw std::invalid_argument("text expression: nesting limit exceeded");
            }
        }
        ~DepthGuard() { --depth_; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;
        std::size_t& depth_;
    };

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

    // Level 12: Set (=), SetDelayed (:=)
    ExprPtr statement() {
        DepthGuard guard(depth_);
        ExprPtr left = replace_all();
        if (take_token(":=")) return make_normal("SetDelayed", {left, statement()});
        if (take_token("=")) return make_normal("Set", {left, statement()});
        return left;
    }

    // Level 11: ReplaceAll (/.)
    ExprPtr replace_all() {
        ExprPtr left = rule();
        while (take_token("/.")) {
            left = make_normal("ReplaceAll", {left, rule()});
        }
        return left;
    }

    // Level 10: Rule (->)
    ExprPtr rule() {
        ExprPtr left = logical_or();
        if (take_token("->")) return make_normal("Rule", {left, rule()});
        return left;
    }

    // Level 9: Or (||)
    ExprPtr logical_or() {
        ExprPtr left = logical_and();
        while (take_token("||")) {
            left = make_normal("Or", {left, logical_and()});
        }
        return left;
    }

    // Level 8: And (&&)
    ExprPtr logical_and() {
        ExprPtr left = relation();
        while (take_token("&&")) {
            left = make_normal("And", {left, relation()});
        }
        return left;
    }

    // Level 7: Relations (==, !=, <=, >=, <, >)
    ExprPtr relation() {
        ExprPtr left = sum();
        constexpr std::pair<std::string_view, std::string_view> operators[] = {
            {"==", "Equal"}, {"!=", "Unequal"}, {"<=", "LessEqual"},
            {">=", "GreaterEqual"}, {"<", "Less"}, {">", "Greater"},
        };
        for (const auto& [token, head] : operators) {
            if (take_token(token)) return make_normal(head, {left, sum()});
        }
        return left;
    }

    // Level 6: Plus (+), Minus (-)
    ExprPtr sum() {
        ExprPtr left = product();
        while (true) {
            if (take('+')) left = plus(left, product());
            else if (peek() == '-' && input_.substr(pos_, 2) != "->" && take('-'))
                left = subtract(left, product());
            else return left;
        }
    }

    // Level 5: Times (*), Divide (/), Juxtaposition
    ExprPtr product() {
        ExprPtr left = dot();
        while (true) {
            if (take('*')) left = times(left, dot());
            else if (peek() == '/' && input_.substr(pos_, 2) != "/." && take('/'))
                left = divide(left, dot());
            else {
                // Juxtaposition (2x, a b) denotes multiplication.
                char c = peek();
                if (c == '(' || c == '[' || std::isalpha(static_cast<unsigned char>(c)))
                    left = times(left, dot());
                else return left;
            }
        }
    }

    // Level 4: Dot (.)
    ExprPtr dot() {
        ExprPtr left = unary();
        while (true) {
            space();
            if (pos_ < input_.size() && input_[pos_] == '.') {
                if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '.') {
                    error("invalid double dot syntax");
                }
                if (pos_ + 1 < input_.size() && input_[pos_ + 1] != '/' &&
                    !std::isdigit(static_cast<unsigned char>(input_[pos_ + 1]))) {
                    ++pos_;
                    left = make_normal("Dot", {left, unary()});
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        return left;
    }

    // Level 3: Unary +, -, Not (!)
    ExprPtr unary() {
        DepthGuard guard(depth_);
        if (take('+')) return unary();
        if (take('-')) return negate(unary());
        if (take('!')) return make_normal("Not", {unary()});
        return exponent();
    }

    // Level 2: Power (^)
    ExprPtr exponent() {
        ExprPtr base = primary();
        if (take('^')) return power(base, unary());  // right associative
        return base;
    }

    // Level 1: Primes ('), Factorial (!), Calls f[x]
    ExprPtr primary() {
        ExprPtr core = atom();
        while (true) {
            space();
            // Primes for derivatives: f'[x] -> Derivative(1, f)[x], y''[t] -> Derivative(2, y)[t]
            if (pos_ < input_.size() && input_[pos_] == '\'') {
                std::size_t count = 0;
                while (pos_ < input_.size() && input_[pos_] == '\'') {
                    ++count;
                    ++pos_;
                }
                core = make_normal("Derivative", {make_integer(static_cast<long long>(count)), core});
                // If followed immediately by arguments e.g. f'[x], apply arguments
                if (peek() == '[') {
                    ++pos_;
                    ExprList args;
                    if (!take(']')) {
                        do { args.push_back(statement()); } while (take(','));
                        expect(']');
                    }
                    core = make_normal(core, std::move(args));
                }
            } else if (pos_ < input_.size() && input_[pos_] == '!' &&
                       (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '=')) { // Factorial postfix !
                ++pos_;
                core = make_normal("Factorial", {core});
            } else {
                break;
            }
        }
        return core;
    }

    ExprPtr atom() {
        char c = peek();
        if (input_.substr(pos_, 2) == "..") {
            error("invalid double dot syntax");
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && pos_ + 1 < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[pos_ + 1])))) {
            std::size_t start = pos_;
            while (pos_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
            std::size_t whole_end = pos_;
            if (pos_ < input_.size() && input_[pos_] == '.' &&
                pos_ + 1 < input_.size() &&
                std::isdigit(static_cast<unsigned char>(input_[pos_ + 1]))) {
                ++pos_;
                std::size_t fractional_start = pos_;
                while (pos_ < input_.size() &&
                       std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
                std::string digits(input_.substr(start, whole_end - start));
                digits += input_.substr(fractional_start, pos_ - fractional_start);
                std::string denominator = "1" + std::string(pos_ - fractional_start, '0');
                return make_rational(Integer::from_string(digits),
                                     Integer::from_string(denominator));
            }
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
        // Blank alone: _
        if (c == '_' && (pos_ + 1 >= input_.size() || !std::isalnum(static_cast<unsigned char>(input_[pos_ + 1])))) {
            ++pos_;
            return make_normal("Blank", {});
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_++;
            while (pos_ < input_.size() &&
                   (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) ++pos_;
            std::string name(input_.substr(start, pos_ - start));

            // Pattern parsing: x_, x_Integer, x__, x___, _h
            if (name[0] == '_' && name.size() > 1 && std::isalpha(static_cast<unsigned char>(name[1]))) {
                std::string head_type = head_for(name.substr(1));
                return make_normal("Blank", {make_symbol(head_type)});
            }
            std::size_t underscore_pos = name.find('_');
            if (underscore_pos != std::string::npos && underscore_pos > 0) {
                std::string sym = name.substr(0, underscore_pos);
                std::string rest = name.substr(underscore_pos);
                if (rest == "___") {
                    return make_normal("Pattern", {make_symbol(sym), make_normal("BlankNullSequence", {})});
                } else if (rest == "__") {
                    return make_normal("Pattern", {make_symbol(sym), make_normal("BlankSequence", {})});
                } else if (rest == "_") {
                    return make_normal("Pattern", {make_symbol(sym), make_normal("Blank", {})});
                } else if (rest.size() > 1 && rest[0] == '_' && std::isalpha(static_cast<unsigned char>(rest[1]))) {
                    std::string head_type = head_for(rest.substr(1));
                    return make_normal("Pattern", {make_symbol(sym), make_normal("Blank", {make_symbol(head_type)})});
                }
            }

            // Calls require no whitespace before '[' in Mathematica syntax (e.g. Sin[x]).
            if (pos_ >= input_.size() || input_[pos_] != '[') {
                if (name == "pi") name = "Pi";
                else if (name == "e") name = "E";
                else if (name == "i") name = "I";
                else if (name == "inf" || name == "infinity") name = "Infinity";
                return make_symbol(std::move(name));
            }
            ++pos_;
            ExprList args;
            if (!take(']')) {
                do { args.push_back(statement()); } while (take(','));
                expect(']');
            }
            return call(name, std::move(args));
        }
        if (take('(')) {
            ExprPtr inside = statement();
            expect(')');
            return inside;
        }
        if (take('{')) {
            ExprList items;
            if (!take('}')) {
                do { items.push_back(statement()); } while (take(','));
                expect('}');
            }
            return make_normal("List", std::move(items));
        }
        error("expected expression");
    }
};

std::string_view infix_token(const ExprPtr& e) {
    if (e->size() != 2) return {};
    constexpr std::pair<std::string_view, std::string_view> tokens[] = {
        {"Rule", "->"}, {"Set", "="}, {"SetDelayed", ":="},
        {"ReplaceAll", "/."}, {"Equal", "=="}, {"Unequal", "!="}, {"Less", "<"},
        {"LessEqual", "<="}, {"Greater", ">"}, {"GreaterEqual", ">="},
        {"And", "&&"}, {"Or", "||"}, {"Dot", "."},
    };
    for (const auto& [head, token] : tokens) {
        if (e->has_head(head)) return token;
    }
    return {};
}

bool negative_term(const ExprPtr& e) {
    if (e->is_number()) return e->number().sign() < 0;
    return e->has_head("Times") && e->size() > 0 &&
           e->arg(0)->is_number() && e->arg(0)->number().sign() < 0;
}

int precedence(const ExprPtr& e) {
    if (e->has_head("Set") || e->has_head("SetDelayed")) return 2;
    if (e->has_head("ReplaceAll")) return 3;
    if (e->has_head("Rule")) return 4;
    if (e->has_head("Or")) return 5;
    if (e->has_head("And")) return 6;
    if (e->has_head("Equal") || e->has_head("Unequal") ||
        e->has_head("Less") || e->has_head("LessEqual") ||
        e->has_head("Greater") || e->has_head("GreaterEqual")) return 7;
    if (e->has_head("Plus")) return 10;
    if (e->has_head("Times")) return 20;
    if (e->has_head("Dot")) return 30;
    if (e->has_head("Power")) return 40;
    return 50;
}

std::string write(const ExprPtr& e, int parent, std::size_t depth) {
    if (depth > 256) throw std::invalid_argument("text expression: nesting limit exceeded");
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
    } else if (e->has_head("Blank") && e->size() == 0) {
        out = "_";
    } else if (e->has_head("Blank") && e->size() == 1 && e->arg(0)->is_symbol()) {
        out = "_" + text_for_head(e->arg(0)->name());
    } else if (e->has_head("Pattern") && e->size() == 2 && e->arg(0)->is_symbol()) {
        std::string sym = e->arg(0)->name();
        const ExprPtr& b = e->arg(1);
        if (b->has_head("Blank") && b->size() == 0) out = sym + "_";
        else if (b->has_head("Blank") && b->size() == 1 && b->arg(0)->is_symbol())
            out = sym + "_" + text_for_head(b->arg(0)->name());
        else if (b->has_head("BlankSequence")) out = sym + "__";
        else if (b->has_head("BlankNullSequence")) out = sym + "___";
        else out = write(e->head(), 0, depth + 1) + "(" + write(e->arg(0), 0, depth + 1) + ", " + write(e->arg(1), 0, depth + 1) + ")";
    } else if (e->has_head("Derivative") && e->size() == 2 && e->arg(0)->is_integer()) {
        auto n = e->arg(0)->integer().to_int64();
        std::string primes = (n && *n > 0 && *n <= 5) ? std::string(static_cast<std::size_t>(*n), '\'') : "";
        if (!primes.empty()) {
            out = write(e->arg(1), 50, depth + 1) + primes;
        } else {
            out = "Derivative[" + write(e->arg(0), 0, depth + 1) + ", " + write(e->arg(1), 0, depth + 1) + "]";
        }
    } else if (e->head()->has_head("Derivative") && e->head()->size() == 2 && e->head()->arg(0)->is_integer()) {
        auto n = e->head()->arg(0)->integer().to_int64();
        std::string primes = (n && *n > 0 && *n <= 5) ? std::string(static_cast<std::size_t>(*n), '\'') : "";
        if (!primes.empty()) {
            out = write(e->head()->arg(1), 50, depth + 1) + primes + "[";
            for (std::size_t i = 0; i < e->size(); ++i) {
                if (i) out += ", ";
                out += write(e->arg(i), 0, depth + 1);
            }
            out += "]";
        } else {
            out = write(e->head(), 0, depth + 1) + "[";
            for (std::size_t i = 0; i < e->size(); ++i) {
                if (i) out += ", ";
                out += write(e->arg(i), 0, depth + 1);
            }
            out += "]";
        }
    } else if (e->has_head("Not") && e->size() == 1) {
        out = "!" + write(e->arg(0), 50, depth + 1);
    } else if (e->has_head("Factorial") && e->size() == 1) {
        out = write(e->arg(0), 50, depth + 1) + "!";
    } else if (e->has_head("Plus") && e->size() > 0) {
        for (std::size_t i = 0; i < e->size(); ++i) {
            const bool negative = negative_term(e->arg(i));
            if (i) out += negative ? " - " : " + ";
            else if (negative) out += "-";
            out += write(negative ? negate(e->arg(i)) : e->arg(i), own + 1, depth + 1);
        }
    } else if (e->has_head("Times") && negative_term(e)) {
        ExprPtr positive = negate(e);
        own = positive->has_head("Times") ? 20 : 30;
        out = "-" + write(positive, own, depth + 1);
    } else if (e->has_head("Times") && e->size() > 0) {
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += "*";
            out += write(e->arg(i), own + 1, depth + 1);
        }
    } else if (e->has_head("Power") && e->size() == 2) {
        std::string base = write(e->arg(0), own + 1, depth + 1);
        if (e->arg(0)->is_integer() && e->arg(0)->integer().is_negative())
            base = "(" + base + ")";
        out = base + "^" + write(e->arg(1), own, depth + 1);
    } else if (const auto token = infix_token(e); !token.empty()) {
        out = write(e->arg(0), own + 1, depth + 1);
        out += " ";
        out += token;
        out += " ";
        const bool right_associative = token == "->" || token == "=" || token == ":=";
        out += write(e->arg(1), own + (right_associative ? 0 : 1), depth + 1);
    } else if (e->has_head("List")) {
        out = "{";
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0, depth + 1);
        }
        out += "}";
    } else {
        bool ordinary_head = e->head()->is_symbol() &&
            write(e->head(), 0, depth + 1) == e->head()->name() &&
            head_for(e->head()->name()) == e->head()->name() &&
            e->head()->name() != "sqrt" && e->head()->name() != "root" &&
            e->head()->name() != "ExprApply" &&
            !(e->head()->name() == "D" && e->size() == 3 && e->arg(2)->is_integer());
        out = ordinary_head ? text_for_head(e->head()->name()) : "ExprApply";
        if (ordinary_head && e->head()->name() == "Integrate" &&
            e->size() >= 4 && (e->size() - 1) % 3 == 0) out = "Integrate";
        if (ordinary_head && (e->head()->name() == "Sum" ||
                              e->head()->name() == "Product" ||
                              e->head()->name() == "Series") && e->size() == 4)
            out = e->head()->name();
        out += "[";
        if (!ordinary_head) {
            out += write(e->head(), 0, depth + 1);
            if (e->size()) out += ", ";
        }
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0, depth + 1);
        }
        out += "]";
    }
    return own < parent ? "(" + out + ")" : out;
}
}  // namespace

ExprPtr parse_text(std::string_view input) { return Parser(input).parse(); }
std::string to_text(const ExprPtr& expr) {
    if (!expr) throw std::invalid_argument("to_text: null expression");
    return write(expr, 0, 0);
}

}  // namespace symats
