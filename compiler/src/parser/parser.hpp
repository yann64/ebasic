#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"

#include <initializer_list>
#include <vector>

namespace ebasic {

/// A straightforward recursive-descent parser, one method per grammar
/// production (parseIf, parseFor, parseAdditive, ...) - the public surface
/// is just the constructor and parseModule(); everything else is parsing
/// machinery private to this translation unit's own grammar.
class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticEngine& diags);

    /// Parses the entire token stream (as produced by Lexer::tokenize) into
    /// a single Module - the top-level entry point used by every driver
    /// (ebc, docgen).
    Module parseModule();

private:
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokenKind kind) const;
    bool checkAny(std::initializer_list<TokenKind> kinds) const;
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind, const std::string& message);
    void skipNewlines();
    void expectStmtEnd();
    void synchronize();
    /// M7: consumes consecutive DocComment tokens (each followed by exactly
    /// one Newline - a blank line inside the block is not tolerated),
    /// joining their text with "\n". Returns "" if none are present. Called
    /// at the top of parseModule()'s loop; the joined text is attached to
    /// the following statement only if its kind is one of the
    /// "documentable" top-level kinds - see ast.hpp's Stmt::docComment.
    std::string collectDocComment();
    /// M7: true for the StmtKinds collectDocComment's result may be
    /// attached to (SubDecl/FunctionDecl/TypeDecl/UnionDecl/NamespaceDecl/
    /// Const/Enum) - the same "public API surface" set M5's
    /// Codegen::generateLibraryInterface already treats as exportable.
    static bool isDocumentableKind(StmtKind kind);

    StmtPtr parseStatement();
    StmtPtr parseDim();
    StmtPtr parseRedim();
    StmtPtr parseConst();
    StmtPtr parseEnum();
    StmtPtr parsePrint();
    StmtPtr parseAssign();
    StmtPtr parseIf();
    StmtPtr parseSelectCase();
    StmtPtr parseFor();
    StmtPtr parseDo();
    StmtPtr parseWhile();
    StmtPtr parseGoto();
    StmtPtr parseGosub();
    StmtPtr parseLabel();
    StmtPtr parseExit();
    StmtPtr parseSub();
    StmtPtr parseFunction();
    void parseOptionalCallConv(Stmt& stmt);
    StmtPtr parseCallStmt();
    StmtPtr parseReturn();
    StmtPtr parseRecordDecl();
    StmtPtr parseMethodPrototype();
    StmtPtr parseConstructor();
    StmtPtr parseDestructor();
    StmtPtr parseProperty();
    StmtPtr parseOperatorDecl();
    /// Matches one overloadable binary-operator token (e.g. `+`, `=`,
    /// `Mod`) and returns the corresponding BinOp. False (leaving `out`
    /// untouched) if the current token isn't an overloadable operator.
    bool matchBinOpSymbol(BinOp& out);
    StmtPtr parseNamespaceDecl();
    /// `Extern "C"|"C++" [Lib "name"] ... End Extern` (M4) - yields multiple
    /// top-level Stmts (one per Declare line inside), appended directly to
    /// `out` rather than returned singly. Only ever called from
    /// parseModule()'s top-level loop.
    void parseExternBlock(std::vector<StmtPtr>& out);
    /// One `Declare Sub/Function Name (...) [As type] [Cdecl|Stdcall]
    /// [Alias "x"] [Lib "y"]` line - a bodyless, extern signature (M4/M8f).
    /// `defaultLinkage`/
    /// `defaultLib` come from the enclosing Extern block (if any); a
    /// standalone (non-block) Declare passes "C" (the only linkage a
    /// standalone Declare can produce in real FreeBASIC too) and "".
    StmtPtr parseExternDecl(const std::string& defaultLinkage, const std::string& defaultLib);
    /// `Namespace Name [Alias "realName"] ... End Namespace` nested inside
    /// an Extern block (M4c) - real FreeBASIC's own way to bind a
    /// namespaced C++ function. Appends one NamespaceDecl Stmt to `out`
    /// (matching parseExternBlock's own out-param convention, since this
    /// could in principle also hold more than one Stmt in the future).
    void parseExternNamespace(std::vector<StmtPtr>& out, const std::string& linkage,
                               const std::string& lib);
    /// Shared-library support: a real `Sub`/`Function ... End Sub`/`End
    /// Function` definition written inside an `Extern "C" ... End Extern`
    /// block - reuses parseSub()/parseFunction() verbatim, then marks the
    /// result `isExported` (see its own doc comment in parser.cpp).
    StmtPtr parseExternExportDef(const std::string& linkage, const std::string& lib);
    std::vector<Param> parseParamList();

    /// Parses an Identifier's trailing chain of `.field` (Member) and, for
    /// the last segment only, an optional `(args)` (a possibly-qualified
    /// Call - `base` becomes the qualifier). Shared by parsePrimary (read
    /// position) and parseAssign (lvalue position).
    ExprPtr parseMemberOrCallChain(ExprPtr base);

    /// Parses statements (skipping blank lines) until the next token is one
    /// of `terminators` or end-of-input.
    std::vector<StmtPtr> parseBlockUntil(std::initializer_list<TokenKind> terminators);

    ExprPtr parseExpr();
    ExprPtr parseXor();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseNot();
    ExprPtr parseRelational();
    ExprPtr parseConcat();
    ExprPtr parseAdditive();
    ExprPtr parseShift();
    ExprPtr parseMod();
    ExprPtr parseIDiv();
    ExprPtr parseMulDiv();
    ExprPtr parseNegate();
    ExprPtr parsePow();
    ExprPtr parseUnaryPtrOps();
    ExprPtr parsePrimary();

    static ExprPtr makeBinary(BinOp op, SourceLoc loc, ExprPtr lhs, ExprPtr rhs);

    Type parseTypeKeyword();
    Type parseFunctionPointerType();

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    DiagnosticEngine& diags_;
    /// Canonical name of the FUNCTION currently being parsed, or empty. Lets
    /// parseAssign recognize `FuncName = value` as a return-value assignment
    /// rather than a regular variable assignment.
    std::string currentFunctionName_;
    /// Library names collected from `Lib "name"` clauses (M4), moved into
    /// Module::externLibs once parseModule() finishes.
    std::vector<std::string> externLibs_;
};

} // namespace ebasic
