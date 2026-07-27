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
    Dot,
    At,     // '@' address-of prefix operator
    Arrow,  // '->' member-through-pointer, desugared by the parser
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
    KwConst,
    KwEnum,
    KwSub,
    KwFunction,
    KwByVal,
    KwByRef,
    KwReturn,
    KwCall,
    KwRedim,
    KwPreserve,
    KwGosub,
    KwType,
    KwNamespace,
    KwPtr,
    KwAny,
    KwUnion,
    KwDeclare,
    KwConstructor,
    KwDestructor,
    KwThis,
    KwExtends,
    KwVirtual,
    KwOverride,
    KwBase,
    KwProperty,
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
    // `lineMap[i]` gives the true {fileId, line} of `source`'s (i+1)-th
    // line, as produced by preprocess() - since `source` may be a flattened
    // multi-file result, the lexer's own line_ counter (over `source`) is
    // translated through this map rather than used directly, so tokens (and
    // therefore diagnostics) report their real originating file/line.
    Lexer(std::string source, const std::vector<SourceLoc>& lineMap, DiagnosticEngine& diags);

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
    SourceLoc currentLoc() const;

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    const std::vector<SourceLoc>& lineMap_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
