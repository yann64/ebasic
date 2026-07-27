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

bool Parser::checkAny(std::initializer_list<TokenKind> kinds) const {
    for (TokenKind k : kinds) {
        if (check(k)) return true;
    }
    return false;
}

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

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Dim;
    stmt->loc = loc;
    stmt->name = name;

    if (match(TokenKind::LParen)) {
        stmt->isArray = true;
        ExprPtr first = parseExpr();
        if (match(TokenKind::KwTo)) {
            stmt->arrayLower = std::move(first);
            stmt->arrayUpper = parseExpr();
        } else {
            stmt->arrayUpper = std::move(first); // lower bound defaults to 0
        }
        expect(TokenKind::RParen, "expected ')' after array bounds");
    }

    expect(TokenKind::KwAs, "expected AS after variable name");
    stmt->declaredType = parseTypeKeyword();

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseConst() {
    SourceLoc loc = peek().loc;
    advance(); // CONST
    const Token& nameTok = expect(TokenKind::Identifier, "expected constant name after CONST");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Const;
    stmt->loc = loc;
    stmt->name = nameTok.text;

    if (match(TokenKind::KwAs)) {
        stmt->declaredType = parseTypeKeyword();
    }
    expect(TokenKind::Equals, "expected '=' in CONST declaration");
    stmt->expr = parseExpr();

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseEnum() {
    SourceLoc loc = peek().loc;
    advance(); // ENUM
    const Token& nameTok = expect(TokenKind::Identifier, "expected enum name after ENUM");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Enum;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    expectStmtEnd();
    skipNewlines();

    while (!check(TokenKind::KwEnd) && !check(TokenKind::End)) {
        const Token& memberTok = expect(TokenKind::Identifier, "expected an enumerator name");
        EnumMember member;
        member.name = memberTok.text;
        member.loc = memberTok.loc;
        if (match(TokenKind::Equals)) {
            member.value = parseExpr();
        }
        expectStmtEnd();
        stmt->enumMembers.push_back(std::move(member));
        skipNewlines();
    }

    expect(TokenKind::KwEnd, "expected END ENUM");
    expect(TokenKind::KwEnum, "expected END ENUM");
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

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Assign;
    stmt->loc = loc;
    stmt->name = name;

    bool isReturnAssign =
        !currentFunctionName_.empty() && canonicalName(name) == currentFunctionName_;
    if (!isReturnAssign && match(TokenKind::LParen)) {
        stmt->index = parseExpr();
        expect(TokenKind::RParen, "expected ')' after array index");
    }

    expect(TokenKind::Equals, "expected '=' in assignment");
    stmt->expr = parseExpr();
    stmt->isReturnAssign = isReturnAssign;

    expectStmtEnd();
    return stmt;
}

std::vector<StmtPtr> Parser::parseBlockUntil(std::initializer_list<TokenKind> terminators) {
    std::vector<StmtPtr> stmts;
    skipNewlines();
    while (!checkAny(terminators) && !check(TokenKind::End)) {
        StmtPtr stmt = parseStatement();
        if (stmt) stmts.push_back(std::move(stmt));
        skipNewlines();
    }
    return stmts;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenKind::KwDim)) return parseDim();
    if (check(TokenKind::KwConst)) return parseConst();
    if (check(TokenKind::KwEnum)) return parseEnum();
    if (check(TokenKind::KwPrint)) return parsePrint();
    if (check(TokenKind::KwIf)) return parseIf();
    if (check(TokenKind::KwSelect)) return parseSelectCase();
    if (check(TokenKind::KwFor)) return parseFor();
    if (check(TokenKind::KwDo)) return parseDo();
    if (check(TokenKind::KwWhile)) return parseWhile();
    if (check(TokenKind::KwGoto)) return parseGoto();
    if (check(TokenKind::KwExit)) return parseExit();
    if (check(TokenKind::KwSub)) return parseSub();
    if (check(TokenKind::KwFunction)) return parseFunction();
    if (check(TokenKind::KwCall)) return parseCallStmt();
    if (check(TokenKind::KwReturn)) return parseReturn();
    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Newline &&
        peek(1).text == ":") {
        return parseLabel();
    }
    if (check(TokenKind::Identifier)) return parseAssign();

    diags_.error(peek().loc, "expected a statement");
    synchronize();
    return nullptr;
}

StmtPtr Parser::parseIf() {
    SourceLoc loc = peek().loc;
    advance(); // IF

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::If;
    stmt->loc = loc;

    stmt->conditions.push_back(parseExpr());
    expect(TokenKind::KwThen, "expected THEN after IF condition");
    expectStmtEnd(); // block-form IF only; single-line IF...THEN is not supported yet

    stmt->blocks.push_back(parseBlockUntil({TokenKind::KwElseIf, TokenKind::KwElse, TokenKind::KwEnd}));

    while (check(TokenKind::KwElseIf)) {
        advance();
        stmt->conditions.push_back(parseExpr());
        expect(TokenKind::KwThen, "expected THEN after ELSEIF condition");
        expectStmtEnd();
        stmt->blocks.push_back(
            parseBlockUntil({TokenKind::KwElseIf, TokenKind::KwElse, TokenKind::KwEnd}));
    }

    if (check(TokenKind::KwElse)) {
        advance();
        expectStmtEnd();
        stmt->blocks.push_back(parseBlockUntil({TokenKind::KwEnd}));
        stmt->hasElse = true;
    }

    expect(TokenKind::KwEnd, "expected END IF");
    expect(TokenKind::KwIf, "expected END IF");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseSelectCase() {
    SourceLoc loc = peek().loc;
    advance(); // SELECT
    expect(TokenKind::KwCase, "expected CASE after SELECT");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::SelectCase;
    stmt->loc = loc;
    stmt->expr = parseExpr();
    expectStmtEnd();
    skipNewlines();

    while (check(TokenKind::KwCase)) {
        advance();
        CaseArm arm;
        if (check(TokenKind::KwElse)) {
            advance();
            arm.isElse = true;
        } else {
            arm.matches.push_back(parseExpr());
            while (match(TokenKind::Comma)) {
                arm.matches.push_back(parseExpr());
            }
        }
        expectStmtEnd();
        arm.body = parseBlockUntil({TokenKind::KwCase, TokenKind::KwEnd});
        stmt->cases.push_back(std::move(arm));
    }

    expect(TokenKind::KwEnd, "expected END SELECT");
    expect(TokenKind::KwSelect, "expected END SELECT");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseFor() {
    SourceLoc loc = peek().loc;
    advance(); // FOR
    const Token& nameTok = expect(TokenKind::Identifier, "expected loop variable name after FOR");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::ForNext;
    stmt->loc = loc;
    stmt->name = nameTok.text;

    expect(TokenKind::Equals, "expected '=' after FOR loop variable");
    stmt->expr = parseExpr();
    expect(TokenKind::KwTo, "expected TO in FOR statement");
    stmt->forEnd = parseExpr();
    if (match(TokenKind::KwStep)) {
        stmt->forStep = parseExpr();
    }
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwNext});
    expect(TokenKind::KwNext, "expected NEXT to close FOR");
    if (check(TokenKind::Identifier)) {
        if (peek().text != stmt->name) {
            diags_.error(peek().loc,
                         "NEXT variable '" + peek().text + "' does not match FOR variable '" +
                             stmt->name + "'");
        }
        advance();
    }
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseDo() {
    SourceLoc loc = peek().loc;
    advance(); // DO

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::DoLoop;
    stmt->loc = loc;

    if (match(TokenKind::KwWhile)) {
        stmt->preTest = LoopTest::While;
        stmt->preCond = parseExpr();
    } else if (match(TokenKind::KwUntil)) {
        stmt->preTest = LoopTest::Until;
        stmt->preCond = parseExpr();
    }
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwLoop});
    expect(TokenKind::KwLoop, "expected LOOP to close DO");

    if (match(TokenKind::KwWhile)) {
        stmt->postTest = LoopTest::While;
        stmt->postCond = parseExpr();
    } else if (match(TokenKind::KwUntil)) {
        stmt->postTest = LoopTest::Until;
        stmt->postCond = parseExpr();
    }
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseWhile() {
    SourceLoc loc = peek().loc;
    advance(); // WHILE

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::WhileWend;
    stmt->loc = loc;
    stmt->expr = parseExpr();
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwWend});
    expect(TokenKind::KwWend, "expected WEND to close WHILE");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseGoto() {
    SourceLoc loc = peek().loc;
    advance(); // GOTO
    const Token& nameTok = expect(TokenKind::Identifier, "expected a label name after GOTO");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Goto;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseLabel() {
    SourceLoc loc = peek().loc;
    std::string name = advance().text; // identifier
    advance(); // the ':' (lexed as a Newline-kind separator token)

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Label;
    stmt->loc = loc;
    stmt->name = name;
    return stmt;
}

StmtPtr Parser::parseExit() {
    SourceLoc loc = peek().loc;
    advance(); // EXIT

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::ExitLoop;
    stmt->loc = loc;

    if (match(TokenKind::KwFor)) stmt->exitKind = LoopKind::For;
    else if (match(TokenKind::KwDo)) stmt->exitKind = LoopKind::Do;
    else if (match(TokenKind::KwWhile)) stmt->exitKind = LoopKind::While;
    else if (match(TokenKind::KwSub)) stmt->exitKind = LoopKind::Sub;
    else if (match(TokenKind::KwFunction)) stmt->exitKind = LoopKind::Function;
    else diags_.error(peek().loc, "expected FOR, DO, WHILE, SUB, or FUNCTION after EXIT");

    expectStmtEnd();
    return stmt;
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    if (!match(TokenKind::LParen)) return params;
    if (!check(TokenKind::RParen)) {
        for (;;) {
            SourceLoc loc = peek().loc;
            bool explicitByRef = false;
            bool explicitByVal = false;
            if (match(TokenKind::KwByRef)) explicitByRef = true;
            else if (match(TokenKind::KwByVal)) explicitByVal = true;

            const Token& nameTok = expect(TokenKind::Identifier, "expected a parameter name");
            expect(TokenKind::KwAs, "expected AS after parameter name");
            TypeKind type = parseTypeKeyword();

            bool byRef = explicitByRef || (!explicitByVal && type == TypeKind::StringT);

            Param p;
            p.name = nameTok.text;
            p.type = type;
            p.byRef = byRef;
            p.loc = loc;
            params.push_back(std::move(p));

            if (!match(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RParen, "expected ')' after parameter list");
    return params;
}

StmtPtr Parser::parseSub() {
    SourceLoc loc = peek().loc;
    advance(); // SUB
    const Token& nameTok = expect(TokenKind::Identifier, "expected a name after SUB");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::SubDecl;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    stmt->params = parseParamList();
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwEnd});
    expect(TokenKind::KwEnd, "expected END SUB");
    expect(TokenKind::KwSub, "expected END SUB");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseFunction() {
    SourceLoc loc = peek().loc;
    advance(); // FUNCTION
    const Token& nameTok = expect(TokenKind::Identifier, "expected a name after FUNCTION");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::FunctionDecl;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    stmt->params = parseParamList();
    expect(TokenKind::KwAs, "expected AS <return type> after FUNCTION parameter list");
    stmt->declaredType = parseTypeKeyword();
    expectStmtEnd();

    std::string outerFunction = currentFunctionName_;
    currentFunctionName_ = canonicalName(stmt->name);
    stmt->body = parseBlockUntil({TokenKind::KwEnd});
    currentFunctionName_ = outerFunction;

    expect(TokenKind::KwEnd, "expected END FUNCTION");
    expect(TokenKind::KwFunction, "expected END FUNCTION");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseCallStmt() {
    SourceLoc loc = peek().loc;
    advance(); // CALL
    const Token& nameTok = expect(TokenKind::Identifier, "expected a procedure name after CALL");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::CallStmt;
    stmt->loc = loc;
    stmt->name = nameTok.text;

    if (match(TokenKind::LParen)) {
        if (!check(TokenKind::RParen)) {
            stmt->args.push_back(parseExpr());
            while (match(TokenKind::Comma)) {
                stmt->args.push_back(parseExpr());
            }
        }
        expect(TokenKind::RParen, "expected ')' after call arguments");
    }

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseReturn() {
    SourceLoc loc = peek().loc;
    advance(); // RETURN

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Return;
    stmt->loc = loc;

    if (!check(TokenKind::Newline) && !check(TokenKind::End)) {
        stmt->expr = parseExpr();
    }

    expectStmtEnd();
    return stmt;
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
        if (match(TokenKind::LParen)) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Call;
            expr->loc = tok.loc;
            expr->stringValue = tok.text;
            if (!check(TokenKind::RParen)) {
                expr->args.push_back(parseExpr());
                while (match(TokenKind::Comma)) {
                    expr->args.push_back(parseExpr());
                }
            }
            expect(TokenKind::RParen, "expected ')'");
            return expr;
        }
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
