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

Type Parser::parseTypeKeyword() {
    Type base;
    if (match(TokenKind::KwByte)) base = TypeKind::Byte;
    else if (match(TokenKind::KwUByte)) base = TypeKind::UByte;
    else if (match(TokenKind::KwShort)) base = TypeKind::Short;
    else if (match(TokenKind::KwUShort)) base = TypeKind::UShort;
    else if (match(TokenKind::KwInteger)) base = TypeKind::Integer;
    else if (match(TokenKind::KwLong)) base = TypeKind::Long;
    else if (match(TokenKind::KwUInteger)) base = TypeKind::UInteger;
    else if (match(TokenKind::KwLongInt)) base = TypeKind::LongInt;
    else if (match(TokenKind::KwULongInt)) base = TypeKind::ULongInt;
    else if (match(TokenKind::KwSingle)) base = TypeKind::Single;
    else if (match(TokenKind::KwDouble)) base = TypeKind::Double;
    else if (match(TokenKind::KwBoolean)) base = TypeKind::Boolean;
    else if (match(TokenKind::KwString)) base = TypeKind::StringT;
    else if (match(TokenKind::KwAny)) {
        // ANY PTR is FB's untyped/void*-equivalent pointer; ANY alone is not
        // a usable type. Represented as Pointer with a null pointee.
        expect(TokenKind::KwPtr, "expected PTR after ANY");
        base.kind = TypeKind::Pointer;
        base.pointee = nullptr;
        // Fall through to the trailing-PTR loop below (ANY PTR PTR is legal
        // per FB docs: an Any Ptr Ptr may be dereferenced to yield an Any Ptr).
    } else if (check(TokenKind::Identifier)) {
        // A user-defined TYPE name. Whether this identifier actually names a
        // declared TYPE is a Sema question, not a parser one - same
        // deferred-disambiguation philosophy as the Call expression (array
        // read vs. function call), resolved by looking up what it names.
        base.kind = TypeKind::UserDefined;
        base.typeName = advance().text;
    } else {
        diags_.error(peek().loc,
                     "expected a type name (BYTE, UBYTE, SHORT, USHORT, INTEGER, LONG, UINTEGER, "
                     "LONGINT, ULONGINT, SINGLE, DOUBLE, BOOLEAN, STRING, ANY PTR, or a declared "
                     "TYPE name)");
        return TypeKind::Unknown;
    }

    // Postfix PTR suffix(es): `Type PTR` and multi-level `Type PTR PTR`.
    while (match(TokenKind::KwPtr)) {
        Type wrapped;
        wrapped.kind = TypeKind::Pointer;
        wrapped.pointee = std::make_shared<Type>(base);
        base = wrapped;
    }
    return base;
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
        // Empty parens - DIM arr() AS Type - declares a dynamic array (size
        // 0 until REDIM'd) rather than a fixed-size one.
        if (!check(TokenKind::RParen)) {
            ExprPtr first = parseExpr();
            if (match(TokenKind::KwTo)) {
                stmt->arrayLower = std::move(first);
                stmt->arrayUpper = parseExpr();
            } else {
                stmt->arrayUpper = std::move(first); // lower bound defaults to 0
            }
        }
        expect(TokenKind::RParen, "expected ')' after array bounds");
    }

    expect(TokenKind::KwAs, "expected AS after variable name");
    stmt->declaredType = parseTypeKeyword();

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseRedim() {
    SourceLoc loc = peek().loc;
    advance(); // REDIM
    bool preserve = match(TokenKind::KwPreserve);
    const Token& nameTok = expect(TokenKind::Identifier, "expected array name after REDIM");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Redim;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    stmt->preserve = preserve;

    expect(TokenKind::LParen, "expected '(' after array name in REDIM");
    ExprPtr first = parseExpr();
    if (match(TokenKind::KwTo)) {
        stmt->arrayLower = std::move(first);
        stmt->arrayUpper = parseExpr();
    } else {
        stmt->arrayUpper = std::move(first); // lower bound defaults to 0
    }
    expect(TokenKind::RParen, "expected ')' after array bounds");

    if (match(TokenKind::KwAs)) {
        stmt->declaredType = parseTypeKeyword();
    }

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

/// Parses a `TYPE`/`UNION` record declaration - the two are structurally
/// identical at this level (a name plus a list of `field AS type` lines);
/// only Sema (the UNION-only "no STRING, directly or nested" restriction)
/// and Codegen (`struct` vs `union`) treat them differently, both by
/// switching on `stmt->kind`. Dispatches on which opening keyword is
/// current rather than duplicating this whole body for each.
StmtPtr Parser::parseRecordDecl() {
    bool isUnion = check(TokenKind::KwUnion);
    TokenKind closingKw = isUnion ? TokenKind::KwUnion : TokenKind::KwType;
    const char* closingWhat = isUnion ? "END UNION" : "END TYPE";

    SourceLoc loc = peek().loc;
    advance(); // TYPE or UNION
    const Token& nameTok = expect(TokenKind::Identifier,
                                   isUnion ? "expected a name after UNION" : "expected a name after TYPE");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = isUnion ? StmtKind::UnionDecl : StmtKind::TypeDecl;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    expectStmtEnd();
    skipNewlines();

    while (!check(TokenKind::KwEnd) && !check(TokenKind::End)) {
        if (check(TokenKind::KwDeclare)) {
            // A method/constructor/destructor prototype - the real
            // definition (body) lives in a separate top-level statement.
            // Left for Sema to reject on a UNION (unions can't have
            // members with constructors/destructors).
            stmt->methods.push_back(parseMethodPrototype());
            skipNewlines();
            continue;
        }
        const Token& fieldTok = expect(TokenKind::Identifier, "expected a field name");
        FieldDecl field;
        field.name = fieldTok.text;
        field.loc = fieldTok.loc;
        expect(TokenKind::KwAs, "expected AS after field name");
        field.type = parseTypeKeyword();
        expectStmtEnd();
        stmt->fields.push_back(std::move(field));
        skipNewlines();
    }

    expect(TokenKind::KwEnd, closingWhat);
    expect(closingKw, closingWhat);
    expectStmtEnd();
    return stmt;
}

/// Parses one `Declare Sub/Function/Constructor/Destructor` line inside a
/// TYPE body - a method's signature only; its body is defined separately,
/// out-of-line, at the top level (see `parseSub`/`parseFunction`'s `.`
/// handling and `parseConstructor`/`parseDestructor`). A Constructor/
/// Destructor has no name of its own (implicitly the owning TYPE) and, in
/// this version, must take no parameters - parameterized construction and
/// constructor overloading are deferred.
StmtPtr Parser::parseMethodPrototype() {
    SourceLoc loc = peek().loc;
    advance(); // DECLARE

    auto stmt = std::make_unique<Stmt>();
    stmt->loc = loc;

    if (match(TokenKind::KwConstructor)) {
        stmt->kind = StmtKind::SubDecl;
        stmt->isCtor = true;
        stmt->params = parseParamList();
        if (!stmt->params.empty()) {
            diags_.error(loc, "parameterized constructors are not supported yet - only "
                               "Declare Constructor() is allowed");
        }
    } else if (match(TokenKind::KwDestructor)) {
        stmt->kind = StmtKind::SubDecl;
        stmt->isDtor = true;
        stmt->params = parseParamList();
        if (!stmt->params.empty()) {
            diags_.error(loc, "a destructor cannot take parameters");
        }
    } else if (match(TokenKind::KwSub)) {
        stmt->kind = StmtKind::SubDecl;
        stmt->name = expect(TokenKind::Identifier, "expected a method name after Declare Sub").text;
        stmt->params = parseParamList();
    } else if (match(TokenKind::KwFunction)) {
        stmt->kind = StmtKind::FunctionDecl;
        stmt->name = expect(TokenKind::Identifier, "expected a method name after Declare Function").text;
        stmt->params = parseParamList();
        expect(TokenKind::KwAs, "expected AS <return type> after Declare Function's parameter list");
        stmt->declaredType = parseTypeKeyword();
    } else {
        diags_.error(peek().loc, "expected SUB, FUNCTION, CONSTRUCTOR, or DESTRUCTOR after DECLARE");
    }

    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseNamespaceDecl() {
    SourceLoc loc = peek().loc;
    advance(); // NAMESPACE
    const Token& nameTok = expect(TokenKind::Identifier, "expected a name after NAMESPACE");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::NamespaceDecl;
    stmt->loc = loc;
    stmt->name = nameTok.text;
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwEnd});
    expect(TokenKind::KwEnd, "expected END NAMESPACE");
    expect(TokenKind::KwNamespace, "expected END NAMESPACE");
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

ExprPtr Parser::parseMemberOrCallChain(ExprPtr base) {
    while (base && (check(TokenKind::Dot) || check(TokenKind::Arrow))) {
        bool isArrow = check(TokenKind::Arrow);
        advance();
        if (isArrow) {
            // `p->field` is pure sugar for `(*p).field` - desugar here so
            // Sema/Codegen need no special-casing for the arrow form.
            auto deref = std::make_unique<Expr>();
            deref->kind = ExprKind::Deref;
            deref->loc = base->loc;
            deref->lhs = std::move(base);
            base = std::move(deref);
        }
        const Token& fieldTok = expect(TokenKind::Identifier, "expected a name after '.' or '->'");
        if (match(TokenKind::LParen)) {
            // A possibly-qualified call: base.Name(args) - base becomes the
            // qualifier (currently only meaningful as a namespace name).
            auto call = std::make_unique<Expr>();
            call->kind = ExprKind::Call;
            call->loc = fieldTok.loc;
            call->stringValue = fieldTok.text;
            call->lhs = std::move(base);
            if (!check(TokenKind::RParen)) {
                call->args.push_back(parseExpr());
                while (match(TokenKind::Comma)) {
                    call->args.push_back(parseExpr());
                }
            }
            expect(TokenKind::RParen, "expected ')'");
            base = std::move(call);
        } else {
            auto member = std::make_unique<Expr>();
            member->kind = ExprKind::Member;
            member->loc = fieldTok.loc;
            member->stringValue = fieldTok.text;
            member->lhs = std::move(base);
            base = std::move(member);
        }
    }
    return base;
}

StmtPtr Parser::parseAssign() {
    SourceLoc loc = peek().loc;

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::Assign;
    stmt->loc = loc;

    if (check(TokenKind::Star)) {
        // `*p = expr` - assignment through a pointer dereference. Reuses
        // the same unary-op parser as read position, so `*p->next = expr`
        // etc. resolve identically on both sides of '='.
        stmt->target = parseUnaryPtrOps();
        expect(TokenKind::Equals, "expected '=' in assignment");
        stmt->expr = parseExpr();
        expectStmtEnd();
        return stmt;
    }

    if (check(TokenKind::KwThis)) {
        // `This.field = expr` - assignment to a member of the current
        // instance, explicitly qualified (needed when a parameter/local
        // shadows the member name; otherwise the bare name already works).
        advance();
        auto thisExpr = std::make_unique<Expr>();
        thisExpr->kind = ExprKind::This;
        thisExpr->loc = loc;
        stmt->target = parseMemberOrCallChain(std::move(thisExpr));
        expect(TokenKind::Equals, "expected '=' in assignment");
        stmt->expr = parseExpr();
        expectStmtEnd();
        return stmt;
    }

    std::string name = advance().text; // identifier
    stmt->name = name;

    bool isReturnAssign =
        !currentFunctionName_.empty() && canonicalName(name) == currentFunctionName_;

    if (!isReturnAssign) {
        ExprPtr base;
        if (match(TokenKind::LParen)) {
            ExprPtr idx = parseExpr();
            expect(TokenKind::RParen, "expected ')' after array index");
            if (check(TokenKind::Dot) || check(TokenKind::Arrow)) {
                // name(idx).field... - becomes a general lvalue chain.
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->loc = loc;
                call->stringValue = name;
                call->args.push_back(std::move(idx));
                base = std::move(call);
            } else {
                stmt->index = std::move(idx); // fast path: name(idx) = expr
            }
        } else if (check(TokenKind::Dot) || check(TokenKind::Arrow)) {
            auto ident = std::make_unique<Expr>();
            ident->kind = ExprKind::Ident;
            ident->loc = loc;
            ident->stringValue = name;
            base = std::move(ident);
        }

        stmt->target = parseMemberOrCallChain(std::move(base));
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
    if (check(TokenKind::KwRedim)) return parseRedim();
    if (check(TokenKind::KwConst)) return parseConst();
    if (check(TokenKind::KwEnum)) return parseEnum();
    if (check(TokenKind::KwType) || check(TokenKind::KwUnion)) return parseRecordDecl();
    if (check(TokenKind::KwNamespace)) return parseNamespaceDecl();
    if (check(TokenKind::KwPrint)) return parsePrint();
    if (check(TokenKind::KwIf)) return parseIf();
    if (check(TokenKind::KwSelect)) return parseSelectCase();
    if (check(TokenKind::KwFor)) return parseFor();
    if (check(TokenKind::KwDo)) return parseDo();
    if (check(TokenKind::KwWhile)) return parseWhile();
    if (check(TokenKind::KwGoto)) return parseGoto();
    if (check(TokenKind::KwGosub)) return parseGosub();
    if (check(TokenKind::KwExit)) return parseExit();
    if (check(TokenKind::KwSub)) return parseSub();
    if (check(TokenKind::KwFunction)) return parseFunction();
    if (check(TokenKind::KwConstructor)) return parseConstructor();
    if (check(TokenKind::KwDestructor)) return parseDestructor();
    if (check(TokenKind::KwCall)) return parseCallStmt();
    if (check(TokenKind::KwReturn)) return parseReturn();
    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Newline &&
        peek(1).text == ":") {
        return parseLabel();
    }
    if (check(TokenKind::Identifier)) return parseAssign();
    if (check(TokenKind::Star)) return parseAssign(); // `*p = expr`
    if (check(TokenKind::KwThis)) return parseAssign(); // `This.field = expr`

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

StmtPtr Parser::parseGosub() {
    SourceLoc loc = peek().loc;
    advance(); // GOSUB
    const Token& nameTok = expect(TokenKind::Identifier, "expected a label name after GOSUB");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::GoSub;
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
            Type type = parseTypeKeyword();

            // FreeBASIC defaults to BYREF for STRING and user-defined TYPE,
            // BYVAL for every other built-in type (verified against docs).
            bool byRef = explicitByRef ||
                         (!explicitByVal && (type.kind == TypeKind::StringT ||
                                             type.kind == TypeKind::UserDefined));

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
    if (match(TokenKind::Dot)) {
        // SUB TypeName.MethodName(...) - an out-of-line method definition,
        // matching real FreeBASIC's "declared within, defined outside" rule.
        stmt->ownerType = nameTok.text;
        stmt->name = expect(TokenKind::Identifier, "expected a method name after '.'").text;
    } else {
        stmt->name = nameTok.text;
    }
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
    if (match(TokenKind::Dot)) {
        // FUNCTION TypeName.MethodName(...) - an out-of-line method definition.
        stmt->ownerType = nameTok.text;
        stmt->name = expect(TokenKind::Identifier, "expected a method name after '.'").text;
    } else {
        stmt->name = nameTok.text;
    }
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

/// `Constructor TypeName (...) ... End Constructor` - the out-of-line
/// definition matching a `Declare Constructor(...)` prototype inside
/// TypeName's body. Named after the owning TYPE itself, not a separate
/// method name (there's only ever one, in this version - no overloading).
StmtPtr Parser::parseConstructor() {
    SourceLoc loc = peek().loc;
    advance(); // CONSTRUCTOR
    const Token& nameTok = expect(TokenKind::Identifier, "expected a TYPE name after CONSTRUCTOR");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::SubDecl;
    stmt->loc = loc;
    stmt->ownerType = nameTok.text;
    stmt->isCtor = true;
    stmt->params = parseParamList();
    if (!stmt->params.empty()) {
        diags_.error(loc, "parameterized constructors are not supported yet");
    }
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwEnd});
    expect(TokenKind::KwEnd, "expected END CONSTRUCTOR");
    expect(TokenKind::KwConstructor, "expected END CONSTRUCTOR");
    expectStmtEnd();
    return stmt;
}

/// `Destructor TypeName () ... End Destructor` - the out-of-line
/// definition matching a `Declare Destructor()` prototype.
StmtPtr Parser::parseDestructor() {
    SourceLoc loc = peek().loc;
    advance(); // DESTRUCTOR
    const Token& nameTok = expect(TokenKind::Identifier, "expected a TYPE name after DESTRUCTOR");

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::SubDecl;
    stmt->loc = loc;
    stmt->ownerType = nameTok.text;
    stmt->isDtor = true;
    stmt->params = parseParamList();
    if (!stmt->params.empty()) {
        diags_.error(loc, "a destructor cannot take parameters");
    }
    expectStmtEnd();

    stmt->body = parseBlockUntil({TokenKind::KwEnd});
    expect(TokenKind::KwEnd, "expected END DESTRUCTOR");
    expect(TokenKind::KwDestructor, "expected END DESTRUCTOR");
    expectStmtEnd();
    return stmt;
}

StmtPtr Parser::parseCallStmt() {
    SourceLoc loc = peek().loc;
    advance(); // CALL

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::CallStmt;
    stmt->loc = loc;

    if (check(TokenKind::KwThis)) {
        // CALL This.Method(args) - one method calling another on itself.
        advance();
        auto qualifier = std::make_unique<Expr>();
        qualifier->kind = ExprKind::This;
        qualifier->loc = loc;
        stmt->target = std::move(qualifier);
        expect(TokenKind::Dot, "expected '.' after THIS");
        stmt->name = expect(TokenKind::Identifier, "expected a method name after 'This.'").text;
    } else {
        const Token& nameTok = expect(TokenKind::Identifier, "expected a procedure name after CALL");
        stmt->name = nameTok.text;

        if (match(TokenKind::Dot)) {
            // CALL Namespace.Name(args) or CALL obj.Method(args) - `target`
            // holds the qualifier (reused from Assign's lvalue-chain field;
            // CallStmt has no assignment target of its own to conflict
            // with), `name` is the final segment.
            auto qualifier = std::make_unique<Expr>();
            qualifier->kind = ExprKind::Ident;
            qualifier->loc = nameTok.loc;
            qualifier->stringValue = nameTok.text;
            stmt->target = std::move(qualifier);
            const Token& finalTok = expect(TokenKind::Identifier, "expected a name after '.'");
            stmt->name = finalTok.text;
        }
    }

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
    ExprPtr left = parseUnaryPtrOps();
    while (check(TokenKind::Caret)) {
        SourceLoc loc = peek().loc;
        advance();
        // Right operand allows a unary minus (2^-3) without letting '^' itself
        // bind looser than negate: -2^2 must still parse as -(2^2).
        left = makeBinary(BinOp::Pow, loc, std::move(left), parseNegate());
    }
    return left;
}

// '@' (address-of) and unary '*' (dereference) bind tighter than '^',
// matching FB's documented precedence table. Right-associative (rare but
// harmless: `@@x`/`**p`).
ExprPtr Parser::parseUnaryPtrOps() {
    if (check(TokenKind::At)) {
        SourceLoc loc = peek().loc;
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::AddressOf;
        expr->loc = loc;
        expr->lhs = parseUnaryPtrOps();
        return expr;
    }
    if (check(TokenKind::Star)) {
        SourceLoc loc = peek().loc;
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::Deref;
        expr->loc = loc;
        expr->lhs = parseUnaryPtrOps();
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
        ExprPtr base;
        if (match(TokenKind::LParen)) {
            auto call = std::make_unique<Expr>();
            call->kind = ExprKind::Call;
            call->loc = tok.loc;
            call->stringValue = tok.text;
            if (!check(TokenKind::RParen)) {
                call->args.push_back(parseExpr());
                while (match(TokenKind::Comma)) {
                    call->args.push_back(parseExpr());
                }
            }
            expect(TokenKind::RParen, "expected ')'");
            base = std::move(call);
        } else {
            auto ident = std::make_unique<Expr>();
            ident->kind = ExprKind::Ident;
            ident->loc = tok.loc;
            ident->stringValue = tok.text;
            base = std::move(ident);
        }

        return parseMemberOrCallChain(std::move(base));
    }
    if (tok.kind == TokenKind::KwThis) {
        advance();
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::This;
        expr->loc = tok.loc;
        return parseMemberOrCallChain(std::move(expr));
    }
    if (match(TokenKind::LParen)) {
        ExprPtr inner = parseExpr();
        expect(TokenKind::RParen, "expected ')'");
        return parseMemberOrCallChain(std::move(inner));
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
