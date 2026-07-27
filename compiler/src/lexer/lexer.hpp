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
    Backslash,
    Caret,
    Amp,
    LParen,
    RParen,
    Equals,
    NotEq,
    Less,
    Greater,
    LessEq,
    GreaterEq,
    Comma,
    KwDim,
    KwAs,
    KwPrint,
    KwByte,
    KwUByte,
    KwShort,
    KwUShort,
    KwInteger,
    KwLong,
    KwUInteger,
    KwLongInt,
    KwULongInt,
    KwSingle,
    KwDouble,
    KwBoolean,
    KwString,
    KwTrue,
    KwFalse,
    KwMod,
    KwAnd,
    KwOr,
    KwXor,
    KwNot,
    KwShl,
    KwShr,
    KwIf,
    KwThen,
    KwElseIf,
    KwElse,
    KwEnd,
    KwSelect,
    KwCase,
    KwFor,
    KwTo,
    KwStep,
    KwNext,
    KwDo,
    KwLoop,
    KwWhile,
    KwWend,
    KwUntil,
    KwGoto,
    KwExit,
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
