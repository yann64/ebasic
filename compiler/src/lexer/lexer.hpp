#pragma once

#include "diagnostics/diagnostics.hpp"

#include <string>
#include <vector>

namespace ebasic {

enum class TokenKind {
    End,
    Newline,
    Identifier,
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    Plus,
    Minus,
    Star,
    Slash,
    Amp,
    LParen,
    RParen,
    Equals,
    Comma,
    KwDim,
    KwAs,
    KwPrint,
    KwInteger,
    KwDouble,
    KwString,
};

struct Token {
    TokenKind kind;
    std::string text;
    long long intValue = 0;
    double doubleValue = 0.0;
    SourceLoc loc;
};

class Lexer {
public:
    Lexer(std::string source, DiagnosticEngine& diags);

    std::vector<Token> tokenize();

private:
    char peek(int offset = 0) const;
    char advance();
    bool isAtEnd() const;

    void skipSpacesAndComments();
    Token makeToken(TokenKind kind, std::string text, SourceLoc loc) const;
    Token lexNumber();
    Token lexString();
    Token lexIdentifierOrKeyword();

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
