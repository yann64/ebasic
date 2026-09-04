#include "preprocessor/pp_expr.hpp"

#include <cctype>
#include <vector>

namespace ebasic {

namespace {

enum class TokKind { Int, Str, Ident, LParen, RParen, Op, End };

struct Tok {
    TokKind kind;
    std::string text;
    long long intValue = 0;
};

std::vector<Tok> tokenize(const std::string& expr) {
    std::vector<Tok> toks;
    size_t i = 0;
    while (i < expr.size()) {
        char c = expr[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i;
            while (i < expr.size() && std::isdigit(static_cast<unsigned char>(expr[i]))) ++i;
            Tok t{TokKind::Int, expr.substr(start, i - start), 0};
            t.intValue = std::stoll(t.text);
            toks.push_back(t);
            continue;
        }
        if (c == '"') {
            // eBasic string literals escape an embedded '"' by doubling it
            // (classic BASIC convention - see Lexer::lexString), not with
            // a backslash - matched here so a stringized macro argument
            // that itself contains quotes round-trips correctly.
            size_t j = i + 1;
            std::string content;
            while (j < expr.size()) {
                if (expr[j] == '"') {
                    if (j + 1 < expr.size() && expr[j + 1] == '"') {
                        content.push_back('"');
                        j += 2;
                        continue;
                    }
                    break;
                }
                content.push_back(expr[j]);
                ++j;
            }
            toks.push_back({TokKind::Str, content, 0});
            i = (j < expr.size()) ? j + 1 : j;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_')) ++i;
            toks.push_back({TokKind::Ident, expr.substr(start, i - start), 0});
            continue;
        }
        if (c == '(') {
            toks.push_back({TokKind::LParen, "(", 0});
            ++i;
            continue;
        }
        if (c == ')') {
            toks.push_back({TokKind::RParen, ")", 0});
            ++i;
            continue;
        }
        if (c == '<' && i + 1 < expr.size() && expr[i + 1] == '>') {
            toks.push_back({TokKind::Op, "<>", 0});
            i += 2;
            continue;
        }
        if (c == '<' && i + 1 < expr.size() && expr[i + 1] == '=') {
            toks.push_back({TokKind::Op, "<=", 0});
            i += 2;
            continue;
        }
        if (c == '>' && i + 1 < expr.size() && expr[i + 1] == '=') {
            toks.push_back({TokKind::Op, ">=", 0});
            i += 2;
            continue;
        }
        if (c == '=' || c == '<' || c == '>' || c == '+' || c == '-' || c == '*' || c == '/') {
            toks.push_back({TokKind::Op, std::string(1, c), 0});
            ++i;
            continue;
        }
        // Unrecognized character: skip it - an incomplete parse further on
        // is what actually gets reported to the caller.
        ++i;
    }
    toks.push_back({TokKind::End, "", 0});
    return toks;
}

bool identIs(const Tok& t, const char* kw) {
    if (t.kind != TokKind::Ident) return false;
    size_t kwLen = std::char_traits<char>::length(kw);
    if (t.text.size() != kwLen) return false;
    for (size_t i = 0; i < t.text.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(t.text[i])) != kw[i]) return false;
    }
    return true;
}

/// An intermediate evaluation result - either an integer or a string,
/// mirroring the two literal kinds the grammar accepts. Comparisons
/// between two of the same kind are always allowed; everything else
/// (arithmetic, ordering, mixed-kind comparison) requires both sides to be
/// integers, and is rejected otherwise.
struct Value {
    bool isString = false;
    long long intVal = 0;
    std::string strVal;

    bool truthy() const { return isString ? !strVal.empty() : intVal != 0; }
};

class Parser {
public:
    Parser(std::vector<Tok> toks, const std::function<void(const std::string&)>& report)
        : toks_(std::move(toks)), report_(report) {}

    std::optional<long long> parse() {
        auto result = parseOr();
        if (!result) return std::nullopt;
        if (cur().kind != TokKind::End) {
            fail("unexpected token '" + cur().text + "' in #if expression");
            return std::nullopt;
        }
        return result->isString ? (result->strVal.empty() ? 0 : 1) : result->intVal;
    }

private:
    std::vector<Tok> toks_;
    size_t pos_ = 0;
    const std::function<void(const std::string&)>& report_;
    bool failed_ = false;

    const Tok& cur() const { return toks_[pos_]; }
    void advance() {
        if (pos_ + 1 < toks_.size()) ++pos_;
    }
    void fail(const std::string& msg) {
        if (!failed_) report_(msg);
        failed_ = true;
    }

    static Value ofInt(long long v) {
        Value r;
        r.intVal = v;
        return r;
    }

    std::optional<Value> parseOr() {
        auto lhs = parseAnd();
        if (!lhs) return std::nullopt;
        while (identIs(cur(), "or")) {
            advance();
            auto rhs = parseAnd();
            if (!rhs) return std::nullopt;
            lhs = ofInt((lhs->truthy() || rhs->truthy()) ? 1 : 0);
        }
        return lhs;
    }

    std::optional<Value> parseAnd() {
        auto lhs = parseNot();
        if (!lhs) return std::nullopt;
        while (identIs(cur(), "and")) {
            advance();
            auto rhs = parseNot();
            if (!rhs) return std::nullopt;
            lhs = ofInt((lhs->truthy() && rhs->truthy()) ? 1 : 0);
        }
        return lhs;
    }

    std::optional<Value> parseNot() {
        if (identIs(cur(), "not")) {
            advance();
            auto operand = parseNot();
            if (!operand) return std::nullopt;
            return ofInt(operand->truthy() ? 0 : 1);
        }
        return parseComparison();
    }

    std::optional<Value> parseComparison() {
        auto lhs = parseAdditive();
        if (!lhs) return std::nullopt;
        if (cur().kind == TokKind::Op &&
            (cur().text == "=" || cur().text == "<>" || cur().text == "<" || cur().text == ">" ||
             cur().text == "<=" || cur().text == ">=")) {
            std::string op = cur().text;
            advance();
            auto rhs = parseAdditive();
            if (!rhs) return std::nullopt;
            if (lhs->isString || rhs->isString) {
                if (!lhs->isString || !rhs->isString) {
                    fail("type mismatch comparing a string and a number in #if expression");
                    return std::nullopt;
                }
                if (op != "=" && op != "<>") {
                    fail("only '=' and '<>' are supported between strings in #if expressions");
                    return std::nullopt;
                }
                bool eq = lhs->strVal == rhs->strVal;
                return ofInt((op == "=") ? (eq ? 1 : 0) : (eq ? 0 : 1));
            }
            long long a = lhs->intVal, b = rhs->intVal;
            if (op == "=") return ofInt(a == b ? 1 : 0);
            if (op == "<>") return ofInt(a != b ? 1 : 0);
            if (op == "<") return ofInt(a < b ? 1 : 0);
            if (op == ">") return ofInt(a > b ? 1 : 0);
            if (op == "<=") return ofInt(a <= b ? 1 : 0);
            return ofInt(a >= b ? 1 : 0);
        }
        return lhs;
    }

    bool requireInt(const Value& v, const char* opName) {
        if (v.isString) {
            fail(std::string("'") + opName + "' requires numeric operands in #if expression");
            return false;
        }
        return true;
    }

    std::optional<Value> parseAdditive() {
        auto lhs = parseTerm();
        if (!lhs) return std::nullopt;
        while (cur().kind == TokKind::Op && (cur().text == "+" || cur().text == "-")) {
            std::string op = cur().text;
            advance();
            auto rhs = parseTerm();
            if (!rhs) return std::nullopt;
            if (!requireInt(*lhs, op.c_str()) || !requireInt(*rhs, op.c_str())) return std::nullopt;
            lhs = ofInt((op == "+") ? (lhs->intVal + rhs->intVal) : (lhs->intVal - rhs->intVal));
        }
        return lhs;
    }

    std::optional<Value> parseTerm() {
        auto lhs = parseUnary();
        if (!lhs) return std::nullopt;
        while ((cur().kind == TokKind::Op && (cur().text == "*" || cur().text == "/")) || identIs(cur(), "mod")) {
            bool isMod = identIs(cur(), "mod");
            std::string op = isMod ? "mod" : cur().text;
            advance();
            auto rhs = parseUnary();
            if (!rhs) return std::nullopt;
            if (!requireInt(*lhs, op.c_str()) || !requireInt(*rhs, op.c_str())) return std::nullopt;
            if (op == "*") {
                lhs = ofInt(lhs->intVal * rhs->intVal);
            } else {
                if (rhs->intVal == 0) {
                    fail("division by zero in #if expression");
                    return std::nullopt;
                }
                lhs = ofInt((op == "/") ? (lhs->intVal / rhs->intVal) : (lhs->intVal % rhs->intVal));
            }
        }
        return lhs;
    }

    std::optional<Value> parseUnary() {
        if (cur().kind == TokKind::Op && cur().text == "-") {
            advance();
            auto operand = parseUnary();
            if (!operand) return std::nullopt;
            if (!requireInt(*operand, "-")) return std::nullopt;
            return ofInt(-operand->intVal);
        }
        return parsePrimary();
    }

    std::optional<Value> parsePrimary() {
        if (cur().kind == TokKind::Int) {
            long long v = cur().intValue;
            advance();
            return ofInt(v);
        }
        if (cur().kind == TokKind::Str) {
            Value r;
            r.isString = true;
            r.strVal = cur().text;
            advance();
            return r;
        }
        if (cur().kind == TokKind::LParen) {
            advance();
            auto inner = parseOr();
            if (!inner) return std::nullopt;
            if (cur().kind != TokKind::RParen) {
                fail("expected ')' in #if expression");
                return std::nullopt;
            }
            advance();
            return inner;
        }
        // An identifier surviving to here (not "and"/"or"/"not"/"mod", which
        // are consumed by their own productions) is an undefined symbol -
        // matching the C preprocessor's own "undefined identifiers are 0"
        // convention, since `defined()` and ordinary macro substitution
        // have already run before this evaluator ever sees the text.
        if (cur().kind == TokKind::Ident) {
            advance();
            return ofInt(0);
        }
        fail("expected a value in #if expression, got '" + cur().text + "'");
        return std::nullopt;
    }
};

} // namespace

std::optional<long long> evalPreprocessorExpr(const std::string& expr,
                                               const std::function<void(const std::string&)>& report) {
    Parser parser(tokenize(expr), report);
    return parser.parse();
}

} // namespace ebasic
