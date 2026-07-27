#include "lexer/lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace ebasic {

namespace {

std::string toUpper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return r;
}

} // namespace

Lexer::Lexer(std::string source, DiagnosticEngine& diags)
    : source_(std::move(source)), diags_(diags) {}

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

char Lexer::peek(int offset) const {
    size_t p = pos_ + static_cast<size_t>(offset);
    if (p >= source_.size()) return '\0';
    return source_[p];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

Token Lexer::makeToken(TokenKind kind, std::string text, SourceLoc loc) const {
    Token t;
    t.kind = kind;
    t.text = std::move(text);
    t.loc = loc;
    return t;
}

void Lexer::skipSpacesAndComments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\'') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '_' && (peek(1) == '\n' || (peek(1) == '\r' && peek(2) == '\n'))) {
            advance();
            if (peek() == '\r') advance();
            if (peek() == '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::lexNumber() {
    SourceLoc loc{line_, col_};
    size_t start = pos_;
    bool isDouble = false;
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isDouble = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    std::string text = source_.substr(start, pos_ - start);
    Token t = makeToken(isDouble ? TokenKind::DoubleLiteral : TokenKind::IntLiteral, text, loc);
    if (isDouble) {
        t.doubleValue = std::stod(text);
    } else {
        t.intValue = std::stoll(text);
    }
    return t;
}

Token Lexer::lexString() {
    SourceLoc loc{line_, col_};
    advance(); // opening quote
    std::string value;
    for (;;) {
        if (isAtEnd() || peek() == '\n') {
            diags_.error(loc, "unterminated string literal");
            break;
        }
        if (peek() == '"') {
            if (peek(1) == '"') {
                value.push_back('"');
                advance();
                advance();
                continue;
            }
            advance(); // closing quote
            break;
        }
        value.push_back(advance());
    }
    return makeToken(TokenKind::StringLiteral, value, loc);
}

Token Lexer::lexIdentifierOrKeyword() {
    SourceLoc loc{line_, col_};
    size_t start = pos_;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
    std::string text = source_.substr(start, pos_ - start);
    std::string upper = toUpper(text);

    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"DIM", TokenKind::KwDim},         {"AS", TokenKind::KwAs},
        {"PRINT", TokenKind::KwPrint},     {"BYTE", TokenKind::KwByte},
        {"UBYTE", TokenKind::KwUByte},     {"SHORT", TokenKind::KwShort},
        {"USHORT", TokenKind::KwUShort},   {"INTEGER", TokenKind::KwInteger},
        {"LONG", TokenKind::KwLong},       {"UINTEGER", TokenKind::KwUInteger},
        {"LONGINT", TokenKind::KwLongInt}, {"ULONGINT", TokenKind::KwULongInt},
        {"SINGLE", TokenKind::KwSingle},   {"DOUBLE", TokenKind::KwDouble},
        {"BOOLEAN", TokenKind::KwBoolean}, {"STRING", TokenKind::KwString},
        {"TRUE", TokenKind::KwTrue},       {"FALSE", TokenKind::KwFalse},
        {"MOD", TokenKind::KwMod},         {"AND", TokenKind::KwAnd},
        {"OR", TokenKind::KwOr},           {"XOR", TokenKind::KwXor},
        {"NOT", TokenKind::KwNot},         {"SHL", TokenKind::KwShl},
        {"SHR", TokenKind::KwShr},
    };

    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return makeToken(it->second, text, loc);
    }
    return makeToken(TokenKind::Identifier, text, loc);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
        skipSpacesAndComments();
        SourceLoc loc{line_, col_};

        if (isAtEnd()) {
            tokens.push_back(makeToken(TokenKind::End, "", loc));
            break;
        }

        char c = peek();

        if (c == '\n') {
            advance();
            tokens.push_back(makeToken(TokenKind::Newline, "\n", loc));
            continue;
        }
        if (c == ':') {
            advance();
            tokens.push_back(makeToken(TokenKind::Newline, ":", loc));
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
            continue;
        }
        if (c == '"') {
            tokens.push_back(lexString());
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        switch (c) {
            case '+': advance(); tokens.push_back(makeToken(TokenKind::Plus, "+", loc)); continue;
            case '-': advance(); tokens.push_back(makeToken(TokenKind::Minus, "-", loc)); continue;
            case '*': advance(); tokens.push_back(makeToken(TokenKind::Star, "*", loc)); continue;
            case '/': advance(); tokens.push_back(makeToken(TokenKind::Slash, "/", loc)); continue;
            case '\\': advance(); tokens.push_back(makeToken(TokenKind::Backslash, "\\", loc)); continue;
            case '^': advance(); tokens.push_back(makeToken(TokenKind::Caret, "^", loc)); continue;
            case '&': advance(); tokens.push_back(makeToken(TokenKind::Amp, "&", loc)); continue;
            case '(': advance(); tokens.push_back(makeToken(TokenKind::LParen, "(", loc)); continue;
            case ')': advance(); tokens.push_back(makeToken(TokenKind::RParen, ")", loc)); continue;
            case '=': advance(); tokens.push_back(makeToken(TokenKind::Equals, "=", loc)); continue;
            case '<':
                advance();
                if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenKind::NotEq, "<>", loc)); }
                else if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::LessEq, "<=", loc)); }
                else { tokens.push_back(makeToken(TokenKind::Less, "<", loc)); }
                continue;
            case '>':
                advance();
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::GreaterEq, ">=", loc)); }
                else { tokens.push_back(makeToken(TokenKind::Greater, ">", loc)); }
                continue;
            case ',': advance(); tokens.push_back(makeToken(TokenKind::Comma, ",", loc)); continue;
            default:
                diags_.error(loc, std::string("unexpected character '") + c + "'");
                advance();
                continue;
        }
    }
    return tokens;
}

} // namespace ebasic
