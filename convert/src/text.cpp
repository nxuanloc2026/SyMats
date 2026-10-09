// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/text.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace symats {
namespace {

bool plain_name(std::string_view name) {
    if (name.empty() || !std::isalpha(static_cast<unsigned char>(name.front()))) return false;
    for (char c : name)
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    return true;
}

bool short_blank(const ExprPtr& e) {
    return e->has_head("Blank") &&
           (e->size() == 0 ||
            (e->size() == 1 && e->arg(0)->is_symbol() && plain_name(e->arg(0)->name())));
}

ExprPtr call(std::string_view name, ExprList args) {
    const std::string head(name);
    if (head == "Plus") return plus(std::move(args));
    if (head == "Times") return times(std::move(args));
    if (head == "Power" && args.size() == 2) return power(args[0], args[1]);
    if (head == "Rational" && args.size() == 2 && args[0]->is_integer() &&
        args[1]->is_integer())
        return make_rational(args[0]->integer(), args[1]->integer());
    if (head == "List") return make_normal("List", std::move(args));
    if (name == "Sqrt" && args.size() == 1)
        return power(args[0], make_rational(1, 2));
    if (name == "Root" && args.size() == 2)
        return power(args[0], divide(make_integer(1), args[1]));
    return make_normal(head, std::move(args));
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    ExprPtr parse() {
        ExprPtr result = assignment();
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

    // Precedence Level 12: assignment (=, :=)
    ExprPtr assignment() {
        DepthGuard guard(depth_);
        ExprPtr left = replace_all();
        if (take_token(":=")) return make_normal("SetDelayed", {left, assignment()});
        if (take_token("=")) return make_normal("Set", {left, assignment()});
        return left;
    }

    // Precedence Level 11: /. (ReplaceAll)
    ExprPtr replace_all() {
        ExprPtr left = rule();
        while (take_token("/.")) {
            left = make_normal("ReplaceAll", {left, rule()});
        }
        return left;
    }

    // Precedence Level 10: -> (Rule)
    ExprPtr rule() {
        ExprPtr left = logical_or();
        if (take_token("->")) return make_normal("Rule", {left, rule()});
        return left;
    }

    // Precedence Level 9: || (Or)
    ExprPtr logical_or() {
        ExprPtr left = logical_and();
        while (take_token("||")) {
            ExprPtr right = logical_and();
            if (left->has_head("Or")) {
                ExprList args = left->args();
                args.push_back(right);
                left = make_normal("Or", std::move(args));
            } else {
                left = make_normal("Or", {left, right});
            }
        }
        return left;
    }

    // Precedence Level 8: && (And)
    ExprPtr logical_and() {
        ExprPtr left = relation();
        while (take_token("&&")) {
            ExprPtr right = relation();
            if (left->has_head("And")) {
                ExprList args = left->args();
                args.push_back(right);
                left = make_normal("And", std::move(args));
            } else {
                left = make_normal("And", {left, right});
            }
        }
        return left;
    }

    // Precedence Level 7: relations (==, !=, <=, >=, <, >)
    ExprPtr relation() {
        DepthGuard guard(depth_);
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

    // Precedence Level 6: +, -
    ExprPtr sum() {
        ExprPtr left = product();
        while (true) {
            if (take('+')) left = plus(left, product());
            else if (peek() == '-' && input_.substr(pos_, 2) != "->" && take('-'))
                left = subtract(left, product());
            else return left;
        }
    }

    // Precedence Level 5: *, /, implicit multiplication
    ExprPtr product() {
        ExprPtr left = dot();
        while (true) {
            if (take('*')) left = times(left, dot());
            else if (peek() == '/' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '.') && take('/'))
                left = divide(left, dot());
            else {
                // Juxtaposition, such as 2x or a b, denotes multiplication.
                char c = peek();
                if (c == '(' || c == '{' || c == '`' || c == '_' || std::isalpha(static_cast<unsigned char>(c)))
                    left = times(left, dot());
                else return left;
            }
        }
    }

    // Precedence Level 4: . (Dot)
    ExprPtr dot() {
        ExprPtr left = prefix_unary();
        while (true) {
            if (peek() == '.' &&
                (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '.') &&
                (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '/') &&
                (pos_ + 1 >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[pos_ + 1])))) {
                take('.');
                ExprPtr right = prefix_unary();
                if (left->has_head("Dot")) {
                    ExprList args = left->args();
                    args.push_back(right);
                    left = make_normal("Dot", std::move(args));
                } else {
                    left = make_normal("Dot", {left, right});
                }
            } else {
                break;
            }
        }
        return left;
    }

    // Precedence Level 3: prefix +, -, ! (Not)
    ExprPtr prefix_unary() {
        DepthGuard guard(depth_);
        if (take('+')) return prefix_unary();
        if (take('-')) return negate(prefix_unary());
        if (peek() == '!' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '=')) {
            take('!');
            return make_normal("Not", {prefix_unary()});
        }
        return exponent();
    }

    // Precedence Level 2: ^ (Power)
    ExprPtr exponent() {
        ExprPtr base = atom();
        if (take('^')) return power(base, prefix_unary());  // right associative
        return base;
    }

    // Precedence Level 1: atom, calls f[x], derivative primes y'[t], postfix ! (Factorial)
    ExprPtr atom() {
        ExprPtr head = primary();
        while (true) {
            if (peek() == '\'') {
                std::size_t primes = 0;
                while (take('\'')) ++primes;
                ExprPtr deriv_head = make_normal("Derivative", {make_integer(static_cast<long long>(primes))});
                head = make_normal(std::move(deriv_head), {head});
            } else if (peek() == '!' && (pos_ + 1 >= input_.size() || input_[pos_ + 1] != '=')) {
                take('!');
                head = make_normal("Factorial", {head});
            } else if (take('[')) {
                ExprList args;
                if (!take(']')) {
                    do { args.push_back(assignment()); } while (take(','));
                    expect(']');
                }
                head = head->is_symbol() ? call(head->name(), std::move(args))
                                         : make_normal(std::move(head), std::move(args));
            } else {
                break;
            }
        }
        return head;
    }
    ExprPtr primary() {
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
        if (std::isalpha(static_cast<unsigned char>(c))) {
            std::size_t start = pos_++;
            while (pos_ < input_.size() &&
                   std::isalnum(static_cast<unsigned char>(input_[pos_]))) ++pos_;
            std::string name(input_.substr(start, pos_ - start));
            if (pos_ < input_.size() && input_[pos_] == '_') {
                ++pos_;
                if (pos_ < input_.size() && input_[pos_] == '_')
                    error("sequence patterns are not in Tier 1");
                std::size_t type_start = pos_;
                while (pos_ < input_.size() &&
                       std::isalnum(static_cast<unsigned char>(input_[pos_]))) ++pos_;
                ExprList blank_args;
                if (pos_ > type_start)
                    blank_args.push_back(make_symbol(std::string(input_.substr(type_start, pos_ - type_start))));
                return make_normal("Pattern", {make_symbol(std::move(name)),
                                               make_normal("Blank", std::move(blank_args))});
            }
            return make_symbol(std::move(name));
        }
        if (take('_')) {
            if (pos_ < input_.size() && input_[pos_] == '_')
                error("sequence patterns are not in Tier 1");
            std::size_t start = pos_;
            while (pos_ < input_.size() &&
                   std::isalnum(static_cast<unsigned char>(input_[pos_]))) ++pos_;
            ExprList args;
            if (pos_ > start)
                args.push_back(make_symbol(std::string(input_.substr(start, pos_ - start))));
            return make_normal("Blank", std::move(args));
        }
        if (take('(')) {
            ExprPtr inside = assignment();
            expect(')');
            return inside;
        }
        if (take('{')) {
            ExprList items;
            if (!take('}')) {
                do { items.push_back(assignment()); } while (take(','));
                expect('}');
            }
            return make_normal("List", std::move(items));
        }
        error("expected expression");
    }
};

std::string_view infix_token(const ExprPtr& e) {
    if (e->size() != 2 && !e->has_head("And") && !e->has_head("Or") && !e->has_head("Dot")) return {};
    constexpr std::pair<std::string_view, std::string_view> tokens[] = {
        {"SetDelayed", ":="}, {"Set", "="}, {"ReplaceAll", "/."},
        {"Rule", "->"}, {"Or", "||"}, {"And", "&&"},
        {"Equal", "=="}, {"Unequal", "!="}, {"Less", "<"},
        {"LessEqual", "<="}, {"Greater", ">"}, {"GreaterEqual", ">="},
        {"Dot", "."},
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
    if (e->has_head("Times")) return 15;
    if (e->has_head("Dot")) return 20;
    if (e->has_head("Not")) return 30;
    if (e->has_head("Power")) return 40;
    if (e->has_head("Factorial")) return 50;
    return 50;
}

std::string write(const ExprPtr& e, int parent, std::size_t depth) {
    if (depth > 256) throw std::invalid_argument("text expression: nesting limit exceeded");
    std::string out;
    int own = precedence(e);
    if (e->is_integer()) out = e->integer().to_string();
    else if (e->is_rational()) {
        out = e->rational().to_string();
        own = 15;
    } else if (e->is_symbol()) {
        const auto& name = e->name();
        {
            bool bare = !name.empty() &&
                (std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_');
            for (char c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c))) bare = false;
            }
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
    } else if (e->has_head("Pattern") && e->size() == 2 && e->arg(0)->is_symbol() &&
               plain_name(e->arg(0)->name()) && short_blank(e->arg(1))) {
        out = e->arg(0)->name() + "_";
        if (e->arg(1)->size() == 1 && e->arg(1)->arg(0)->is_symbol())
            out += e->arg(1)->arg(0)->name();
    } else if (short_blank(e)) {
        out = "_";
        if (e->size() == 1 && e->arg(0)->is_symbol()) out += e->arg(0)->name();
    } else if (e->has_head("Plus") && e->size() > 0) {
        for (std::size_t i = 0; i < e->size(); ++i) {
            const bool negative = negative_term(e->arg(i));
            if (i) out += negative ? " - " : " + ";
            else if (negative) out += "-";
            out += write(negative ? negate(e->arg(i)) : e->arg(i), own + 1, depth + 1);
        }
    } else if (e->has_head("Times") && negative_term(e)) {
        ExprPtr positive = negate(e);
        own = positive->has_head("Times") ? 15 : 25;
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
    } else if (e->has_head("Not") && e->size() == 1) {
        out = "!" + write(e->arg(0), own, depth + 1);
    } else if (e->has_head("Factorial") && e->size() == 1) {
        out = write(e->arg(0), own, depth + 1) + "!";
    } else if (e->head()->has_head("Derivative") && e->head()->size() == 1 &&
               e->head()->arg(0)->is_integer() && e->size() == 1) {
        long long n = e->head()->arg(0)->integer().to_int64().value_or(1);
        out = write(e->arg(0), own, depth + 1) + std::string(n > 0 ? n : 1, '\'');
    } else if (const auto token = infix_token(e); !token.empty()) {
        const bool right_associative = token == "->" || token == "=" || token == ":=";
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += " " + std::string(token) + " ";
            out += write(e->arg(i), own + (right_associative ? 0 : 1), depth + 1);
        }
    } else if (e->has_head("List")) {
        out = "{";
        for (std::size_t i = 0; i < e->size(); ++i) {
            if (i) out += ", ";
            out += write(e->arg(i), 0, depth + 1);
        }
        out += "}";
    } else {
        out = write(e->head(), 50, depth + 1);
        out += "[";
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

namespace {

std::string strip_comments(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    std::size_t i = 0;
    int depth = 0;
    while (i < input.size()) {
        if (i + 1 < input.size() && input[i] == '(' && input[i + 1] == '*') {
            depth++;
            i += 2;
        } else if (depth > 0 && i + 1 < input.size() && input[i] == '*' && input[i + 1] == ')') {
            depth--;
            i += 2;
        } else {
            if (depth == 0) {
                result += input[i];
            } else {
                if (input[i] == '\n') result += '\n';
                else result += ' ';
            }
            i++;
        }
    }
    return result;
}

std::string expand_percents(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '%') {
            std::size_t count = 0;
            while (i + count < text.size() && text[i + count] == '%') {
                count++;
            }
            if (count > 0 && i + count < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[i + count]))) {
                std::size_t num_start = i + count;
                std::size_t num_end = num_start;
                while (num_end < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[num_end]))) {
                    num_end++;
                }
                std::string num_str(text.substr(num_start, num_end - num_start));
                result += "Out[" + num_str + "]";
                i = num_end;
            } else {
                result += "Out[-" + std::to_string(count) + "]";
                i += count;
            }
        } else {
            result += text[i++];
        }
    }
    return result;
}

bool ends_with_continuation_operator(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    if (s.empty()) return false;
    char last = s.back();
    if (last == '+' || last == '-' || last == '*' || last == '/' || last == '^' ||
        last == '=' || last == ':' || last == ',' || last == '>') return true;
    if (s.size() >= 2) {
        std::string_view end2 = s.substr(s.size() - 2);
        if (end2 == "->" || end2 == ":=" || end2 == "==" || end2 == "!=" ||
            end2 == "<=" || end2 == ">=" || end2 == "/." || end2 == "&&" || end2 == "||") {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<Statement> parse_cell(std::string_view input) {
    std::string clean_text = strip_comments(input);
    std::vector<Statement> statements;
    std::string cur_stmt;
    int bracket_depth = 0;

    auto finish_statement = [&](bool suppressed) {
        std::size_t start = 0;
        while (start < cur_stmt.size() && std::isspace(static_cast<unsigned char>(cur_stmt[start]))) {
            start++;
        }
        std::size_t end = cur_stmt.size();
        while (end > start && std::isspace(static_cast<unsigned char>(cur_stmt[end - 1]))) {
            end--;
        }
        if (start < end) {
            std::string stmt_str = cur_stmt.substr(start, end - start);
            std::string expanded = expand_percents(stmt_str);
            ExprPtr parsed = parse_text(expanded);
            statements.push_back(Statement{parsed, suppressed});
        }
        cur_stmt.clear();
    };

    std::size_t i = 0;
    while (i < clean_text.size()) {
        char c = clean_text[i];
        if (c == '[' || c == '(' || c == '{') {
            bracket_depth++;
            cur_stmt += c;
            i++;
        } else if (c == ']' || c == ')' || c == '}') {
            if (bracket_depth > 0) bracket_depth--;
            cur_stmt += c;
            i++;
        } else if (c == ';') {
            if (bracket_depth > 0) {
                cur_stmt += c;
                i++;
            } else {
                finish_statement(/*suppressed=*/true);
                i++;
            }
        } else if (c == '\n') {
            if (bracket_depth > 0 || ends_with_continuation_operator(cur_stmt)) {
                cur_stmt += ' ';
                i++;
            } else {
                finish_statement(/*suppressed=*/false);
                i++;
            }
        } else {
            cur_stmt += c;
            i++;
        }
    }
    finish_statement(/*suppressed=*/false);
    return statements;
}

}  // namespace symats
