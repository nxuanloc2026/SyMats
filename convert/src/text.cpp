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
        ExprPtr result = relation();
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
                if (c == '(' || std::isalpha(static_cast<unsigned char>(c)))
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
    ExprPtr atom() {
        ExprPtr head = primary();
        while (take('[')) {
            ExprList args;
            if (!take(']')) {
                do { args.push_back(relation()); } while (take(','));
                expect(']');
            }
            head = head->is_symbol() ? call(head->name(), std::move(args))
                                     : make_normal(std::move(head), std::move(args));
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
            ExprPtr inside = relation();
            expect(')');
            return inside;
        }
        if (take('{')) {
            ExprList items;
            if (!take('}')) {
                do { items.push_back(relation()); } while (take(','));
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

}  // namespace symats
