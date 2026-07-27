#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"

#include <vector>

namespace ebasic {

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticEngine& diags);

    Module parseModule();

private:
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind, const std::string& message);
    void skipNewlines();
    void expectStmtEnd();
    void synchronize();

    StmtPtr parseStatement();
    StmtPtr parseDim();
    StmtPtr parsePrint();
    StmtPtr parseAssign();

    ExprPtr parseExpr();
    ExprPtr parseTerm();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();

    TypeKind parseTypeKeyword();

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
