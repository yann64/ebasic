#include "parser/parser.hpp"

namespace ebasic {

Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& diags)
    : tokens_(std::move(tokens)), diags_(diags) {}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

const Token& Parser::advance() {
    const Token& t = peek();
    if (pos_ + 1 < tokens_.size()) ++pos_;
    return t;
}

bool Parser::check(TokenKind kind) const { return peek().kind == kind; }

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenKind kind, const std::string& message) {
    if (check(kind)) return advance();
    diags_.error(peek().loc, message);
    return peek();
}

void Parser::skipNewlines() {
    while (check(TokenKind::Newline)) advance();
}

void Parser::expectStmtEnd() {
    if (check(TokenKind::Newline)) {
        advance();
        return;
    }
    if (check(TokenKind::End)) return;
    diags_.error(peek().loc, "expected end of statement");
    synchronize();
}

void Parser::synchronize() {
    while (!check(TokenKind::Newline) && !check(TokenKind::End)) advance();
    if (check(TokenKind::Newline)) advance();
}

Module Parser::parseModule() {
    Module module;
    skipNewlines();
    while (!check(TokenKind::End)) {
        StmtPtr stmt = parseStatement();
        if (stmt) module.stmts.push_back(std::move(stmt));
        skipNewlines();
    }
    return module;
}

TypeKind Parser::parseTypeKeyword() {
    if (match(TokenKind::KwInteger)) return TypeKind::Integer;
    if (match(TokenKind::KwDouble)) return TypeKind::Double;
    if (match(TokenKind::KwString)) return TypeKind::StringT;
    diags_.error(peek().loc, "expected a type name (INTEGER, DOUBLE, or STRING)");
    return TypeKind::Unknown;
}

StmtPtr Parser::parseDim() {
    SourceLoc loc = peek().loc;
    advance(); // DIM
    const Token& nameTok = expect(TokenKind::Identifier, "expected variable name after DIM");
    std::string name = nameTok.text;
    expect(TokenKind::KwAs, "expected AS after variable name");
    TypeKind type = parseTypeKeyword();

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Dim;
    stmt->loc = loc;
    stmt->name = name;
    stmt->declaredType = type;

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parsePrint() {
    SourceLoc loc = peek().loc;
    advance(); // PRINT

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Print;
    stmt->loc = loc;

    if (!check(TokenKind::Newline) && !check(TokenKind::End)) {
        stmt->args.push_back(parseExpr());
        while (match(TokenKind::Comma)) {
            stmt->args.push_back(parseExpr());
        }
    }

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseAssign() {
    SourceLoc loc = peek().loc;
    std::string name = advance().text; // identifier
    expect(TokenKind::Equals, "expected '=' in assignment");
    ExprPtr expr = parseExpr();

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Assign;
    stmt->loc = loc;
    stmt->name = name;
    stmt->expr = std::move(expr);

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenKind::KwDim)) return parseDim();
    if (check(TokenKind::KwPrint)) return parsePrint();
    if (check(TokenKind::Identifier)) return parseAssign();

    diags_.error(peek().loc, "expected a statement");
    synchronize();
    return nullptr;
}

ExprPtr Parser::parseExpr() {
    ExprPtr left = parseTerm();
    while (check(TokenKind::Plus) || check(TokenKind::Minus) || check(TokenKind::Amp)) {
        SourceLoc loc = peek().loc;
        BinOp op = check(TokenKind::Plus)    ? BinOp::Add
                   : check(TokenKind::Minus) ? BinOp::Sub
                                             : BinOp::Concat;
        advance();
        ExprPtr right = parseTerm();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->loc = loc;
        bin->binOp = op;
        bin->lhs = std::move(left);
        bin->rhs = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseTerm() {
    ExprPtr left = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        SourceLoc loc = peek().loc;
        BinOp op = check(TokenKind::Star) ? BinOp::Mul : BinOp::Div;
        advance();
        ExprPtr right = parseUnary();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->loc = loc;
        bin->binOp = op;
        bin->lhs = std::move(left);
        bin->rhs = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenKind::Minus)) {
        SourceLoc loc = peek().loc;
        advance();
        ExprPtr operand = parseUnary();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UnaryNeg;
        expr->loc = loc;
        expr->lhs = std::move(operand);
        return expr;
    }
    return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
    const Token& tok = peek();

    if (tok.kind == TokenKind::IntLiteral) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::IntLiteral;
        expr->loc = tok.loc;
        expr->intValue = tok.intValue;
        return expr;
    }
    if (tok.kind == TokenKind::DoubleLiteral) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::DoubleLiteral;
        expr->loc = tok.loc;
        expr->doubleValue = tok.doubleValue;
        return expr;
    }
    if (tok.kind == TokenKind::StringLiteral) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::StringLiteral;
        expr->loc = tok.loc;
        expr->stringValue = tok.text;
        return expr;
    }
    if (tok.kind == TokenKind::Identifier) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::Ident;
        expr->loc = tok.loc;
        expr->stringValue = tok.text;
        return expr;
    }
    if (match(TokenKind::LParen)) {
        ExprPtr inner = parseExpr();
        expect(TokenKind::RParen, "expected ')'");
        return inner;
    }

    diags_.error(tok.loc, "expected an expression");
    advance();
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::IntLiteral;
    expr->loc = tok.loc;
    expr->intValue = 0;
    return expr;
}

} // namespace ebasic
