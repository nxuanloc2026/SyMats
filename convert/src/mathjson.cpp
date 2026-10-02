// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats/mathjson.h"

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace symats {
namespace {

constexpr unsigned kMaxDepth = 256;

struct Json {
    enum class Kind { Number, String, Array, Object, Other } kind;
    std::string text;
    std::vector<Json> array;
    std::map<std::string, Json> object;
};

void append_utf8(std::string& out, unsigned cp) {
    if (cp <= 0x7f) out += static_cast<char>(cp);
    else if (cp <= 0x7ff) {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp <= 0xffff) {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

class Reader {
public:
    explicit Reader(std::string_view source) : source_(source) {}
    Json read() {
        Json result = value(0);
        space();
        if (pos_ != source_.size()) fail("trailing input");
        return result;
    }

private:
    std::string_view source_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const char* message) const {
        throw std::invalid_argument(std::string("MathJSON at byte ") +
                                    std::to_string(pos_) + ": " + message);
    }
    void space() {
        while (pos_ < source_.size() &&
               (source_[pos_] == ' ' || source_[pos_] == '\t' ||
                source_[pos_] == '\n' || source_[pos_] == '\r')) ++pos_;
    }
    bool take(char c) {
        space();
        if (pos_ < source_.size() && source_[pos_] == c) { ++pos_; return true; }
        return false;
    }
    unsigned hex4() {
        unsigned n = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ == source_.size()) fail("short Unicode escape");
            char c = source_[pos_++];
            n <<= 4;
            if (c >= '0' && c <= '9') n |= c - '0';
            else if (c >= 'a' && c <= 'f') n |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') n |= c - 'A' + 10;
            else fail("bad Unicode escape");
        }
        return n;
    }
    std::string string() {
        if (!take('"')) fail("expected string");
        std::string out;
        while (pos_ < source_.size()) {
            unsigned char c = static_cast<unsigned char>(source_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (pos_ == source_.size()) fail("unfinished escape");
            char e = source_[pos_++];
            switch (e) {
            case '"': case '\\': case '/': out += e; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp = hex4();
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    if (pos_ + 2 > source_.size() ||
                        source_.substr(pos_, 2) != "\\u") fail("missing low surrogate");
                    pos_ += 2;
                    unsigned low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) fail("bad low surrogate");
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) fail("orphan low surrogate");
                append_utf8(out, cp);
                break;
            }
            default: fail("bad string escape");
            }
        }
        fail("unterminated string");
    }
    Json value(unsigned depth) {
        if (depth > kMaxDepth) fail("nesting limit exceeded");
        space();
        if (pos_ == source_.size()) fail("expected value");
        char c = source_[pos_];
        if (c == '"') return {Json::Kind::String, string(), {}, {}};
        if (c == '[') {
            ++pos_;
            Json result{Json::Kind::Array, {}, {}, {}};
            if (take(']')) return result;
            do { result.array.push_back(value(depth + 1)); } while (take(','));
            if (!take(']')) fail("expected ]");
            return result;
        }
        if (c == '{') {
            ++pos_;
            Json result{Json::Kind::Object, {}, {}, {}};
            if (take('}')) return result;
            do {
                std::string key = string();
                if (!take(':')) fail("expected :");
                if (!result.object.emplace(std::move(key), value(depth + 1)).second)
                    fail("duplicate object key");
            } while (take(','));
            if (!take('}')) fail("expected }");
            return result;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            std::string_view word = c == 't' ? "true" : c == 'f' ? "false" : "null";
            if (source_.substr(pos_, word.size()) != word) fail("bad literal");
            pos_ += word.size();
            return {Json::Kind::Other, std::string(word), {}, {}};
        }
        std::size_t start = pos_;
        if (c == '-') ++pos_;
        if (pos_ == source_.size()) fail("bad number");
        if (source_[pos_] == '0') ++pos_;
        else {
            if (source_[pos_] < '1' || source_[pos_] > '9') fail("bad number");
            while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
        }
        if (pos_ < source_.size() && source_[pos_] == '.') {
            ++pos_;
            std::size_t digits = pos_;
            while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
            if (digits == pos_) fail("bad fraction");
        }
        if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < source_.size() && (source_[pos_] == '+' || source_[pos_] == '-')) ++pos_;
            std::size_t digits = pos_;
            while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) ++pos_;
            if (digits == pos_) fail("bad exponent");
        }
        return {Json::Kind::Number, std::string(source_.substr(start, pos_ - start)), {}, {}};
    }
};

bool number_string(std::string_view s) {
    try {
        Json j = Reader(s).read();
        return j.kind == Json::Kind::Number;
    } catch (const std::invalid_argument&) { return false; }
}

ExprPtr number(std::string_view s) {
    // Decimal and scientific MathJSON literals are exact rationals in v0.1.
    std::size_t e = s.find_first_of("eE");
    std::string_view mantissa = s.substr(0, e);
    std::string digits;
    for (char c : mantissa) if (c != '.') digits += c;
    std::size_t dot = mantissa.find('.');
    int fractional = dot == std::string_view::npos ? 0 : static_cast<int>(mantissa.size() - dot - 1);
    int exponent = 0;
    if (e != std::string_view::npos) {
        std::string_view part = s.substr(e + 1);
        bool negative = !part.empty() && part.front() == '-';
        if (!part.empty() && (part.front() == '-' || part.front() == '+')) part.remove_prefix(1);
        for (char c : part) {
            if (exponent > 1000) throw std::invalid_argument("MathJSON exponent too large");
            exponent = exponent * 10 + (c - '0');
        }
        if (negative) exponent = -exponent;
    }
    int scale = fractional - exponent;
    if (scale > 1000 || scale < -1000)
        throw std::invalid_argument("MathJSON exponent too large");
    if (scale < 0) digits.append(static_cast<std::size_t>(-scale), '0');
    Integer n = Integer::from_string(digits);
    if (scale <= 0) return make_integer(std::move(n));
    return make_rational(std::move(n), Integer::from_string("1" + std::string(scale, '0')));
}

const Json* field(const Json& j, const char* key) {
    auto it = j.object.find(key);
    return it == j.object.end() ? nullptr : &it->second;
}

ExprPtr decode(const Json& j, unsigned depth) {
    if (depth > kMaxDepth) throw std::invalid_argument("MathJSON expression nesting limit exceeded");
    if (j.kind == Json::Kind::Number) return number(j.text);
    if (j.kind == Json::Kind::String) {
        if (number_string(j.text)) return number(j.text);
        if (j.text == "ExponentialE") return make_symbol("E");
        if (j.text == "ImaginaryUnit") return make_symbol("I");
        if (j.text == "PositiveInfinity") return make_symbol("Infinity");
        if (j.text.empty() || j.text.front() == '\'')
            throw std::invalid_argument("MathJSON strings are not Expr values");
        if (j.text.front() == '`' && j.text.back() == '`' && j.text.size() > 2)
            return make_symbol(j.text.substr(1, j.text.size() - 2));
        if (!(std::isalpha(static_cast<unsigned char>(j.text.front())) || j.text.front() == '_'))
            throw std::invalid_argument("invalid MathJSON symbol");
        for (unsigned char c : j.text)
            if (!(std::isalnum(c) || c == '_'))
                throw std::invalid_argument("invalid MathJSON symbol");
        return make_symbol(j.text);
    }
    if (j.kind == Json::Kind::Object) {
        if (const Json* n = field(j, "num")) {
            if (n->kind != Json::Kind::String)
                throw std::invalid_argument("unsupported MathJSON number");
            if (n->text == "+Infinity") return make_symbol("Infinity");
            if (n->text == "-Infinity") return negate(make_symbol("Infinity"));
            if (!number_string(n->text)) throw std::invalid_argument("unsupported MathJSON number");
            return number(n->text);
        }
        if (const Json* s = field(j, "sym")) {
            if (s->kind != Json::Kind::String || s->text.empty())
                throw std::invalid_argument("invalid MathJSON symbol");
            return make_symbol(s->text);
        }
        if (const Json* f = field(j, "fn")) {
            if (f->kind != Json::Kind::Array)
                throw std::invalid_argument("invalid MathJSON function");
            return decode(*f, depth + 1);
        }
        throw std::invalid_argument("unsupported MathJSON object");
    }
    if (j.kind != Json::Kind::Array || j.array.empty())
        throw std::invalid_argument("expected MathJSON function");
    const Json& op = j.array.front();
    if (op.kind != Json::Kind::String) throw std::invalid_argument("invalid MathJSON operator");
    const std::string& name = op.text;
    // Canonical Compute Engine integrals wrap the integrand in Function/Block.
    if (name == "Integrate" && j.array.size() >= 3) {
        const Json& f = j.array[1];
        if (f.kind == Json::Kind::Array && f.array.size() == 3 &&
            f.array[0].kind == Json::Kind::String && f.array[0].text == "Function") {
            const Json* body = &f.array[1];
            if (body->kind == Json::Kind::Array && body->array.size() == 2 &&
                body->array[0].kind == Json::Kind::String && body->array[0].text == "Block")
                body = &body->array[1];
            ExprList integral{decode(*body, depth + 1)};
            for (std::size_t i = 2; i < j.array.size(); ++i)
                integral.push_back(decode(j.array[i], depth + 1));
            return make_normal("Integrate", std::move(integral));
        }
    }
    ExprList args;
    for (std::size_t i = 1; i < j.array.size(); ++i)
        args.push_back(decode(j.array[i], depth + 1));
    auto require = [&](std::size_t count) {
        if (args.size() != count) throw std::invalid_argument("wrong MathJSON arity");
    };
    if (name == "Add") return plus(std::move(args));
    if (name == "Multiply") return times(std::move(args));
    if (name == "Power") { require(2); return power(args[0], args[1]); }
    if (name == "Divide") { require(2); return divide(args[0], args[1]); }
    if (name == "Subtract") { require(2); return subtract(args[0], args[1]); }
    if (name == "Negate") { require(1); return negate(args[0]); }
    if (name == "Sqrt") {
        require(1);
        return power(args[0], make_rational(1, 2));
    }
    if (name == "Root") {
        require(2);
        return power(args[0], divide(make_integer(1), args[1]));
    }
    if (name == "Rational") {
        require(2);
        if (!args[0]->is_integer() || !args[1]->is_integer() || args[1]->integer().is_zero())
            throw std::invalid_argument("invalid MathJSON rational");
        return make_rational(args[0]->integer(), args[1]->integer());
    }
    if (name == "Matrix") {
        require(1);
        if (!args[0]->has_head("List")) throw std::invalid_argument("invalid MathJSON matrix");
        return args[0];
    }
    if (name == "Apply") {
        if (args.empty()) throw std::invalid_argument("invalid MathJSON Apply");
        return make_normal(args[0], ExprList(args.begin() + 1, args.end()));
    }
    if ((name == "Tuple" || name == "Limits") && !args.empty())
        return make_normal("List", std::move(args));
    if (name == "Limit") {
        require(3);
        return make_normal("Limit", {args[0], make_normal("Rule", {args[1], args[2]})});
    }
    if (name == "Ln") { require(1); return make_normal("Log", std::move(args)); }
    if (name == "Log" && args.size() == 1)
        return make_normal("Log", {make_integer(10), args[0]});
    if (name == "Log" && args.size() == 2)
        return make_normal("Log", {args[1], args[0]});
    return make_normal(name == "Assign" ? "Set" : name, std::move(args));
}

std::string quote(std::string_view s) {
    std::string out = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 0x20) {
            out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
    return out + '"';
}

std::string encode(const ExprPtr& expr, unsigned depth) {
    if (!expr || depth > kMaxDepth) throw std::invalid_argument("invalid/deep Expr");
    if (expr->is_integer()) return "{\"num\":" + quote(expr->integer().to_string()) + "}";
    if (expr->is_rational())
        return "[\"Rational\"," + encode(make_integer(expr->rational().num()), depth + 1) +
               "," + encode(make_integer(expr->rational().den()), depth + 1) + "]";
    if (expr->is_symbol()) {
        const std::string& s = expr->name();
        if (s == "E") return quote("ExponentialE");
        if (s == "I") return quote("ImaginaryUnit");
        if (s == "Infinity") return quote("PositiveInfinity");
        if (s == "ExponentialE" || s == "ImaginaryUnit" || s == "PositiveInfinity")
            return "{\"sym\":" + quote(s) + "}";
        bool simple = !s.empty() && (std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_');
        for (unsigned char c : s) simple = simple && (std::isalnum(c) || c == '_');
        return simple && !number_string(s) ? quote(s) : "{\"sym\":" + quote(s) + "}";
    }
    const ExprPtr& head = expr->head();
    std::string name;
    bool apply = !head->is_symbol();
    if (!apply) {
        name = head->name();
        if (name == "Plus") name = "Add";
        else if (name == "Times") name = "Multiply";
        else if (name == "Set") name = "Assign";
        else if (name == "Log" && expr->size() == 1) name = "Ln";
    } else name = "Apply";
    if (expr->has_head("Limit") && expr->size() == 2 && expr->arg(1)->has_head("Rule") &&
        expr->arg(1)->size() == 2)
        return "[\"Limit\"," + encode(expr->arg(0), depth + 1) + "," +
               encode(expr->arg(1)->arg(0), depth + 1) + "," +
               encode(expr->arg(1)->arg(1), depth + 1) + "]";
    if (expr->has_head("Log") && expr->size() == 2)
        return "[\"Log\"," + encode(expr->arg(1), depth + 1) + "," +
               encode(expr->arg(0), depth + 1) + "]";
    std::string out = "[" + quote(name);
    if (apply) out += "," + encode(head, depth + 1);
    bool binder = expr->has_head("Integrate") || expr->has_head("NIntegrate") ||
                  expr->has_head("Sum") || expr->has_head("Product") ||
                  expr->has_head("Series");
    for (std::size_t i = 0; i < expr->size(); ++i) {
        const ExprPtr& arg = expr->arg(i);
        if (binder && i > 0 && arg->has_head("List")) {
            out += ",[\"Tuple\"";
            for (const ExprPtr& item : arg->args()) out += "," + encode(item, depth + 1);
            out += "]";
        } else out += "," + encode(arg, depth + 1);
    }
    return out + "]";
}

}  // namespace

ExprPtr parse_mathjson(std::string_view json) { return decode(Reader(json).read(), 0); }
std::string to_mathjson(const ExprPtr& expr) { return encode(expr, 0); }

}  // namespace symats
