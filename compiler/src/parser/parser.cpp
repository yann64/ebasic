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
    if (match(TokenKind::KwByte)) return TypeKind::Byte;
    if (match(TokenKind::KwUByte)) return TypeKind::UByte;
    if (match(TokenKind::KwShort)) return TypeKind::Short;
    if (match(TokenKind::KwUShort)) return TypeKind::UShort;
    if (match(TokenKind::KwInteger)) return TypeKind::Integer;
    if (match(TokenKind::KwLong)) return TypeKind::Long;
    if (match(TokenKind::KwUInteger)) return TypeKind::UInteger;
    if (match(TokenKind::KwLongInt)) return TypeKind::LongInt;
    if (match(TokenKind::KwULongInt)) return TypeKind::ULongInt;
    if (match(TokenKind::KwSingle)) return TypeKind::Single;
    if (match(TokenKind::KwDouble)) return TypeKind::Double;
    if (match(TokenKind::KwBoolean)) return TypeKind::Boolean;
    if (match(TokenKind::KwString)) return TypeKind::StringT;
    diags_.error(peek().loc,
                 "expected a type name (BYTE, UBYTE, SHORT, USHORT, INTEGER, LONG, UINTEGER, "
                 "LONGINT, ULONGINT, SINGLE, DOUBLE, BOOLEAN, or STRING)");
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

// Expression grammar, loosest-binding to tightest-binding, matching
// FreeBASIC's documented operator precedence (highest to lowest):
//   ^  >  unary -  >  * /  >  \  >  MOD  >  SHL/SHR  >  + -  >  &  >
//   relational  >  NOT  >  AND  >  OR  >  XOR
// EQV/IMP/ANDALSO/ORELSE and the CAST/pointer/array/Is tiers are not part of
// this language slice yet.

ExprPtr Parser::makeBinary(BinOp op, SourceLoc loc, ExprPtr lhs, ExprPtr rhs) {
    auto bin = std::make_unique<Expr>();
    bin->kind = ExprKind::Binary;
    bin->loc = loc;
    bin->binOp = op;
    bin->lhs = std::move(lhs);
    bin->rhs = std::move(rhs);
    return bin;
}

ExprPtr Parser::parseExpr() { return parseXor(); }

ExprPtr Parser::parseXor() {
    ExprPtr left = parseOr();
    while (check(TokenKind::KwXor)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::Xor, loc, std::move(left), parseOr());
    }
    return left;
}

ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();
    while (check(TokenKind::KwOr)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::Or, loc, std::move(left), parseAnd());
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    ExprPtr left = parseNot();
    while (check(TokenKind::KwAnd)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::And, loc, std::move(left), parseNot());
    }
    return left;
}

ExprPtr Parser::parseNot() {
    if (check(TokenKind::KwNot)) {
        SourceLoc loc = peek().loc;
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UnaryNot;
        expr->loc = loc;
        expr->lhs = parseNot();
        return expr;
    }
    return parseRelational();
}

ExprPtr Parser::parseRelational() {
    ExprPtr left = parseConcat();
    for (;;) {
        SourceLoc loc = peek().loc;
        BinOp op;
        if (check(TokenKind::Equals)) op = BinOp::Eq;
        else if (check(TokenKind::NotEq)) op = BinOp::Ne;
        else if (check(TokenKind::Less)) op = BinOp::Lt;
        else if (check(TokenKind::LessEq)) op = BinOp::Le;
        else if (check(TokenKind::Greater)) op = BinOp::Gt;
        else if (check(TokenKind::GreaterEq)) op = BinOp::Ge;
        else break;
        advance();
        left = makeBinary(op, loc, std::move(left), parseConcat());
    }
    return left;
}

ExprPtr Parser::parseConcat() {
    ExprPtr left = parseAdditive();
    while (check(TokenKind::Amp)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::Concat, loc, std::move(left), parseAdditive());
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseShift();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        SourceLoc loc = peek().loc;
        BinOp op = check(TokenKind::Plus) ? BinOp::Add : BinOp::Sub;
        advance();
        left = makeBinary(op, loc, std::move(left), parseShift());
    }
    return left;
}

ExprPtr Parser::parseShift() {
    ExprPtr left = parseMod();
    while (check(TokenKind::KwShl) || check(TokenKind::KwShr)) {
        SourceLoc loc = peek().loc;
        BinOp op = check(TokenKind::KwShl) ? BinOp::Shl : BinOp::Shr;
        advance();
        left = makeBinary(op, loc, std::move(left), parseMod());
    }
    return left;
}

ExprPtr Parser::parseMod() {
    ExprPtr left = parseIDiv();
    while (check(TokenKind::KwMod)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::Mod, loc, std::move(left), parseIDiv());
    }
    return left;
}

ExprPtr Parser::parseIDiv() {
    ExprPtr left = parseMulDiv();
    while (check(TokenKind::Backslash)) {
        SourceLoc loc = peek().loc;
        advance();
        left = makeBinary(BinOp::IDiv, loc, std::move(left), parseMulDiv());
    }
    return left;
}

ExprPtr Parser::parseMulDiv() {
    ExprPtr left = parseNegate();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        SourceLoc loc = peek().loc;
        BinOp op = check(TokenKind::Star) ? BinOp::Mul : BinOp::Div;
        advance();
        left = makeBinary(op, loc, std::move(left), parseNegate());
    }
    return left;
}

ExprPtr Parser::parseNegate() {
    if (check(TokenKind::Minus)) {
        SourceLoc loc = peek().loc;
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UnaryNeg;
        expr->loc = loc;
        expr->lhs = parseNegate(); // right-associative: - - x, matches FB
        return expr;
    }
    return parsePow();
}

ExprPtr Parser::parsePow() {
    ExprPtr left = parsePrimary();
    while (check(TokenKind::Caret)) {
        SourceLoc loc = peek().loc;
        advance();
        // Right operand allows a unary minus (2^-3) without letting '^' itself
        // bind looser than negate: -2^2 must still parse as -(2^2).
        left = makeBinary(BinOp::Pow, loc, std::move(left), parseNegate());
    }
    return left;
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
    if (tok.kind == TokenKind::KwTrue || tok.kind == TokenKind::KwFalse) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::BoolLiteral;
        expr->loc = tok.loc;
        expr->intValue = (tok.kind == TokenKind::KwTrue) ? -1 : 0;
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
