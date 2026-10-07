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
        {"plot", "Plot"}, {"paramplot", "ParametricPlot"},
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
    for (const auto& [alias, internal] : aliases) {
        if (head == internal) return std::string(alias);
    }
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
        ExprPtr result = compound();
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
        while (pos_ < input_.size()) {
            if (std::isspace(static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            } else if (input_[pos_] == '(' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '*') {
                pos_ += 2;
                int depth = 1;
                while (pos_ < input_.size() && depth > 0) {
                    if (input_[pos_] == '(' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '*') {
                        ++depth;
                        pos_ += 2;
                    } else if (input_[pos_] == '*' && pos_ + 1 < input_.size() && input_[pos_ + 1] == ')') {
                        --depth;
                        pos_ += 2;
                    } else {
                        ++pos_;
                    }
                }
            } else {
                break;
            }
        }
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
    ExprPtr compound() {
        DepthGuard guard(depth_);
        ExprPtr left = relation();
        if (!take(';')) return left;
        ExprList args{std::move(left)};
        do {
            space();
            if (pos_ == input_.size() || peek() == ')' || peek() == ']' || peek() == '}') {
                args.push_back(make_symbol("Null"));
                break;
            }
            args.push_back(relation());
        } while (take(';'));
        return make_normal("CompoundExpression", std::move(args));
    }
    ExprPtr relation() {
        DepthGuard guard(depth_);
        ExprPtr left = sum();
        if (take_token("->")) return make_normal("Rule", {left, relation()});
        if (take_token(":=")) return make_normal("SetDelayed", {left, relation()});
        constexpr std::pair<std::string_view, std::string_view> operators[] = {
            {"==", "Equal"}, {"!=", "Unequal"}, {"<=", "LessEqual"},
            {">=", "GreaterEqual"}, {"<", "Less"}, {">", "Greater"},
        };
        for (const auto& [token, head] : operators) {
            if (take_token(token)) return make_normal(head, {left, sum()});
        }
        if (take_token("=")) return make_normal("Set", {left, relation()});
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
        DepthGuard guard(depth_);
        if (take('+')) return unary();
        if (take('-')) return negate(unary());
        return exponent();
    }
    ExprPtr exponent() {
        ExprPtr base = atom();
        if (take('^')) return power(base, unary());  // right associative
        return base;
    }
    ExprPtr atom_primary() {
        char c = peek();
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
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_++;
            while (pos_ < input_.size() &&
                   (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
                    input_[pos_] == '_')) ++pos_;
            std::string name(input_.substr(start, pos_ - start));
            if (name == "pi") name = "Pi";
            else if (name == "e") name = "E";
            else if (name == "i") name = "I";
            else if (name == "inf" || name == "infinity") name = "Infinity";
            return make_symbol(std::move(name));
        }
        if (take('%')) {
            if (take('%')) {
                return make_normal("Out", {make_integer(-2)});
            }
            if (peek() == '-') {
                std::size_t start = pos_;
                ++pos_;
                if (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                    while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_])))
                        ++pos_;
                    long long k = std::stoll(std::string(input_.substr(start + 1, pos_ - start - 1)));
                    return make_normal("Out", {make_integer(-k)});
                }
                pos_ = start;
            }
            if (std::isdigit(static_cast<unsigned char>(peek()))) {
                std::size_t start = pos_;
                while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_])))
                    ++pos_;
                long long n = std::stoll(std::string(input_.substr(start, pos_ - start)));
                return make_normal("Out", {make_integer(n)});
            }
            return make_normal("Out", {});
        }
        if (take('(')) {
            ExprPtr inside = compound();
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

    ExprPtr atom() {
        ExprPtr base = atom_primary();
        while (pos_ < input_.size() && input_[pos_] == '(') {
            ++pos_;
            ExprList args;
            if (!take(')')) {
                do { args.push_back(relation()); } while (take(','));
                expect(')');
            }
            if (base->is_symbol()) {
                base = call(base->name(), std::move(args));
            } else {
                base = make_normal(base, std::move(args));
            }
        }
        return base;
    }
};

std::string_view infix_token(const ExprPtr& e) {
    if (e->size() != 2) return {};
    constexpr std::pair<std::string_view, std::string_view> tokens[] = {
        {"Rule", "->"}, {"Set", "="}, {"SetDelayed", ":="},
        {"Equal", "=="}, {"Unequal", "!="}, {"Less", "<"},
        {"LessEqual", "<="}, {"Greater", ">"}, {"GreaterEqual", ">="},
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
    if (!infix_token(e).empty()) return 5;
    if (e->has_head("Plus")) return 10;
    if (e->has_head("Times")) return 20;
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
        out += "(";
        if (!ordinary_head) {
            out += write(e->head(), 0, depth + 1);
            if (e->size()) out += ", ";
        }
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0, depth + 1);
        }
        out += ")";
    }
    return own < parent ? "(" + out + ")" : out;
}
}  // namespace

ExprPtr parse_text(std::string_view input) { return Parser(input).parse(); }
std::string to_text(const ExprPtr& expr) {
    if (!expr) throw std::invalid_argument("to_text: null expression");
    return write(expr, 0, 0);
}

namespace {

bool is_continuation_op(std::string_view token) {
    static constexpr std::string_view ops[] = {
        ":=", "==", "!=", "<=", ">=", "->", "/.", "&&", "||",
        "+", "-", "*", "/", "^", "=", ",", "<", ">"
    };
    for (std::string_view op : ops) {
        if (token == op) return true;
    }
    return false;
}

bool is_continuation(std::string_view text, std::size_t pos) {
    std::size_t prev = pos;
    while (prev > 0 && (text[prev - 1] == ' ' || text[prev - 1] == '\t' || text[prev - 1] == '\r')) {
        --prev;
    }
    if (prev > 0) {
        if (prev >= 2 && is_continuation_op(text.substr(prev - 2, 2))) return true;
        if (is_continuation_op(text.substr(prev - 1, 1))) return true;
    }

    std::size_t next = pos + 1;
    while (next < text.size() && (text[next] == ' ' || text[next] == '\t' || text[next] == '\r' || text[next] == '\n')) {
        ++next;
    }
    if (next < text.size()) {
        if (next + 1 < text.size() && is_continuation_op(text.substr(next, 2))) return true;
        if (is_continuation_op(text.substr(next, 1))) return true;
    }

    return false;
}

}  // namespace

std::vector<Statement> parse_cell(std::string_view cell_text) {
    std::vector<Statement> statements;
    std::size_t start = 0;
    std::size_t pos = 0;
    std::size_t paren_depth = 0;
    std::size_t bracket_depth = 0;
    std::size_t brace_depth = 0;
    int comment_depth = 0;
    bool in_string = false;
    bool in_backtick = false;

    auto strip_comments_and_whitespace = [](std::string_view s) {
        std::size_t p = 0;
        while (p < s.size()) {
            if (std::isspace(static_cast<unsigned char>(s[p]))) {
                ++p;
            } else if (s[p] == '(' && p + 1 < s.size() && s[p + 1] == '*') {
                p += 2;
                int depth = 1;
                while (p < s.size() && depth > 0) {
                    if (s[p] == '(' && p + 1 < s.size() && s[p + 1] == '*') {
                        ++depth;
                        p += 2;
                    } else if (s[p] == '*' && p + 1 < s.size() && s[p + 1] == ')') {
                        --depth;
                        p += 2;
                    } else {
                        ++p;
                    }
                }
            } else {
                break;
            }
        }
        return s.substr(p);
    };

    auto emit_statement = [&](std::size_t end, bool suppressed) {
        std::string_view chunk = cell_text.substr(start, end - start);
        std::size_t c_start = 0;
        while (c_start < chunk.size() && std::isspace(static_cast<unsigned char>(chunk[c_start]))) {
            ++c_start;
        }
        std::size_t c_end = chunk.size();
        while (c_end > c_start && std::isspace(static_cast<unsigned char>(chunk[c_end - 1]))) {
            --c_end;
        }
        std::string_view trimmed = chunk.substr(c_start, c_end - c_start);
        if (strip_comments_and_whitespace(trimmed).empty()) return;

        ExprPtr expr = parse_text(trimmed);
        statements.push_back(Statement{expr, suppressed});
    };

    while (pos < cell_text.size()) {
        char c = cell_text[pos];

        if (comment_depth > 0) {
            if (c == '*' && pos + 1 < cell_text.size() && cell_text[pos + 1] == ')') {
                --comment_depth;
                pos += 2;
                continue;
            }
            if (c == '(' && pos + 1 < cell_text.size() && cell_text[pos + 1] == '*') {
                ++comment_depth;
                pos += 2;
                continue;
            }
            ++pos;
            continue;
        }

        if (in_string) {
            if (c == '\\' && pos + 1 < cell_text.size()) {
                pos += 2;
            } else {
                if (c == '"') in_string = false;
                ++pos;
            }
            continue;
        }

        if (in_backtick) {
            if (c == '`') {
                if (pos + 1 < cell_text.size() && cell_text[pos + 1] == '`') pos += 2;
                else { in_backtick = false; ++pos; }
            } else ++pos;
            continue;
        }

        if (c == '(' && pos + 1 < cell_text.size() && cell_text[pos + 1] == '*') {
            comment_depth = 1;
            pos += 2;
            continue;
        }

        if (c == '"') { in_string = true; ++pos; continue; }
        if (c == '`') { in_backtick = true; ++pos; continue; }

        if (c == '(') { ++paren_depth; ++pos; continue; }
        if (c == ')') { if (paren_depth > 0) --paren_depth; ++pos; continue; }
        if (c == '[') { ++bracket_depth; ++pos; continue; }
        if (c == ']') { if (bracket_depth > 0) --bracket_depth; ++pos; continue; }
        if (c == '{') { ++brace_depth; ++pos; continue; }
        if (c == '}') { if (brace_depth > 0) --brace_depth; ++pos; continue; }

        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            if (c == ';') {
                emit_statement(pos, true);
                ++pos;
                start = pos;
                continue;
            }
            if (c == '\n') {
                if (is_continuation(cell_text, pos)) {
                    ++pos;
                    continue;
                }
                emit_statement(pos, false);
                ++pos;
                start = pos;
                continue;
            }
        }

        ++pos;
    }

    if (start < cell_text.size()) {
        emit_statement(cell_text.size(), false);
    }

    return statements;
}

}  // namespace symats
