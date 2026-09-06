#include "symbols.hpp"

#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include <sstream>
#include <vector>

namespace ebasic::lsp {

nlohmann::json pointRange(const ebasic::SourceLoc& loc) {
    // One character wide - the closest honest approximation available:
    // eBasic's AST tracks a single point per node, never a start/end span.
    return {
        {"start", {{"line", loc.line - 1}, {"character", loc.column - 1}}},
        {"end", {{"line", loc.line - 1}, {"character", loc.column}}},
    };
}

namespace {

/// Small, self-contained copy of Codegen's/docgen's own basicTypeName - not
/// shared, same reasoning docgen/src/render.cpp gives for its own copy:
/// each is a handful of lines, and the "owner" (Codegen, here Sema-index
/// rendering) has no other reason to link against the others.
std::string basicTypeName(const ebasic::Type& type) {
    switch (type.kind) {
        case ebasic::TypeKind::Byte: return "BYTE";
        case ebasic::TypeKind::UByte: return "UBYTE";
        case ebasic::TypeKind::Short: return "SHORT";
        case ebasic::TypeKind::UShort: return "USHORT";
        case ebasic::TypeKind::Integer: return "INTEGER";
        case ebasic::TypeKind::Long: return "LONG";
        case ebasic::TypeKind::UInteger: return "UINTEGER";
        case ebasic::TypeKind::LongInt: return "LONGINT";
        case ebasic::TypeKind::ULongInt: return "ULONGINT";
        case ebasic::TypeKind::Single: return "SINGLE";
        case ebasic::TypeKind::Double: return "DOUBLE";
        case ebasic::TypeKind::Boolean: return "BOOLEAN";
        case ebasic::TypeKind::StringT: return "STRING";
        case ebasic::TypeKind::ZStringT: return "ZSTRING";
        case ebasic::TypeKind::UserDefined: return type.typeName;
        case ebasic::TypeKind::Pointer:
            return (type.pointee ? basicTypeName(*type.pointee) : std::string("ANY")) + " PTR";
        case ebasic::TypeKind::Unknown: return "";
    }
    return "";
}

nlohmann::json makeSymbol(const std::string& name, int kind, const ebasic::SourceLoc& loc) {
    nlohmann::json range = pointRange(loc);
    return {{"name", name}, {"kind", kind}, {"range", range}, {"selectionRange", range}};
}

// LSP SymbolKind values (see the spec's own enum) relevant here.
constexpr int kSymbolKindNamespace = 3;
constexpr int kSymbolKindEnum = 10;
constexpr int kSymbolKindFunction = 12;
constexpr int kSymbolKindConstant = 14;
constexpr int kSymbolKindStruct = 23;

std::string renderProcSignature(bool isFunction, const std::string& name,
                                 const std::vector<ebasic::Param>& params,
                                 const ebasic::Type& returnType) {
    std::ostringstream out;
    out << (isFunction ? "FUNCTION " : "SUB ") << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) out << ", ";
        out << (params[i].byRef ? "BYREF " : "BYVAL ") << params[i].name << " AS "
            << basicTypeName(params[i].type);
    }
    out << ")";
    if (isFunction) out << " AS " << basicTypeName(returnType);
    return out.str();
}

} // namespace

std::optional<CheckedDocument> checkDocument(const std::string& path, const std::string& text,
                                              const std::vector<std::string>& includeDirs) {
    ebasic::DiagnosticEngine diags;
    diags.registerFile(path);

    ebasic::PreprocessResult pre = ebasic::preprocess(text, path, diags, includeDirs);
    if (diags.hasErrors()) return std::nullopt;

    ebasic::Lexer lexer(pre.source, pre.lineMap, diags);
    std::vector<ebasic::Token> tokens = lexer.tokenize();
    if (diags.hasErrors()) return std::nullopt;

    ebasic::Parser parser(std::move(tokens), diags);
    ebasic::Module module = parser.parseModule();
    if (diags.hasErrors()) return std::nullopt;

    ebasic::Sema sema(diags);
    sema.check(module);
    // Sema's collect* passes (module-level SUB/FUNCTION/TYPE registration)
    // always run to completion before any statement body is checked, so a
    // real, useful SemaIndex exists even if body-level type errors remain.
    ebasic::SemaIndex index = sema.index();
    return CheckedDocument{std::move(module), std::move(index), std::move(diags)};
}

std::optional<std::string> identifierAt(const std::string& text, int line, int character) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string lineText = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!lineText.empty() && lineText.back() == '\r') lineText.pop_back();
        lines.push_back(std::move(lineText));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (line < 0 || static_cast<size_t>(line) >= lines.size()) return std::nullopt;

    ebasic::DiagnosticEngine diags; // discarded - a broken line just yields no match below
    diags.registerFile("<hover-line>");
    std::vector<ebasic::SourceLoc> lineMap = {ebasic::SourceLoc{line + 1, 0, 0}};
    ebasic::Lexer lexer(lines[static_cast<size_t>(line)], lineMap, diags);
    std::vector<ebasic::Token> tokens = lexer.tokenize();
    for (const ebasic::Token& tok : tokens) {
        if (tok.kind != ebasic::TokenKind::Identifier) continue;
        int startCol = tok.loc.column - 1;
        int endCol = startCol + static_cast<int>(tok.text.size());
        if (character >= startCol && character < endCol) return tok.text;
    }
    return std::nullopt;
}

nlohmann::json documentSymbols(const ebasic::Module& module) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& stmtPtr : module.stmts) {
        const ebasic::Stmt& stmt = *stmtPtr;
        switch (stmt.kind) {
            case ebasic::StmtKind::TypeDecl:
            case ebasic::StmtKind::UnionDecl:
                result.push_back(makeSymbol(stmt.name, kSymbolKindStruct, stmt.loc));
                break;
            case ebasic::StmtKind::Const:
                result.push_back(makeSymbol(stmt.name, kSymbolKindConstant, stmt.loc));
                break;
            case ebasic::StmtKind::Enum:
                result.push_back(makeSymbol(stmt.name, kSymbolKindEnum, stmt.loc));
                break;
            case ebasic::StmtKind::NamespaceDecl:
                result.push_back(makeSymbol(stmt.name, kSymbolKindNamespace, stmt.loc));
                break;
            case ebasic::StmtKind::SubDecl:
            case ebasic::StmtKind::FunctionDecl:
                // A TYPE method's out-of-line definition (ownerType set) is
                // not a top-level symbol of its own - mirrors docgen's own
                // isFreeProc rule.
                if (stmt.ownerType.empty()) {
                    result.push_back(makeSymbol(stmt.name, kSymbolKindFunction, stmt.loc));
                }
                break;
            default:
                break;
        }
    }
    return result;
}

std::optional<nlohmann::json> hoverFor(const ebasic::SemaIndex& index, const std::string& rawName) {
    const std::string key = ebasic::canonicalName(rawName);
    if (auto it = index.procedures.find(key); it != index.procedures.end()) {
        const ebasic::ProcedureInfo& proc = it->second;
        std::string sig = renderProcSignature(proc.isFunction, rawName, proc.params, proc.returnType);
        return nlohmann::json{{"contents", {{"kind", "plaintext"}, {"value", sig}}}};
    }
    if (auto it = index.structs.find(key); it != index.structs.end()) {
        std::string sig = "TYPE " + rawName;
        if (!it->second.baseName.empty()) sig += " EXTENDS " + it->second.baseName;
        return nlohmann::json{{"contents", {{"kind", "plaintext"}, {"value", sig}}}};
    }
    if (auto it = index.symbols.find(key); it != index.symbols.end()) {
        const ebasic::SymbolInfo& sym = it->second;
        std::string sig = (sym.isConst ? "CONST " : "DIM ") + rawName + " AS " + basicTypeName(sym.type);
        if (sym.isArray) sig += "()";
        return nlohmann::json{{"contents", {{"kind", "plaintext"}, {"value", sig}}}};
    }
    return std::nullopt;
}

std::optional<ebasic::SourceLoc> declLocFor(const ebasic::SemaIndex& index, const std::string& rawName) {
    const std::string key = ebasic::canonicalName(rawName);
    if (auto it = index.procedures.find(key); it != index.procedures.end()) return it->second.declLoc;
    if (auto it = index.structs.find(key); it != index.structs.end()) return it->second.declLoc;
    if (auto it = index.symbols.find(key); it != index.symbols.end()) return it->second.declLoc;
    return std::nullopt;
}

namespace {

// Every reserved keyword the lexer recognizes (lexer.cpp's own `keywords`
// table, kept in sync by hand - a small, presentation-only duplication:
// unlike parsing/name-resolution logic, a completion list is just data,
// the same reasoning docgen's own basicTypeName copy already relies on).
const std::vector<std::string>& reservedKeywords() {
    static const std::vector<std::string> keywords = {
        "DIM", "AS", "PRINT", "BYTE", "UBYTE", "SHORT", "USHORT", "INTEGER", "LONG", "UINTEGER",
        "LONGINT", "ULONGINT", "SINGLE", "DOUBLE", "BOOLEAN", "STRING", "TRUE", "FALSE", "MOD", "AND",
        "OR", "XOR", "NOT", "SHL", "SHR", "IF", "THEN", "ELSEIF", "ELSE", "END", "SELECT", "CASE",
        "FOR", "TO", "STEP", "NEXT", "DO", "LOOP", "WHILE", "WEND", "UNTIL", "GOTO", "EXIT", "CONST",
        "ENUM", "SUB", "FUNCTION", "BYVAL", "BYREF", "RETURN", "CALL", "REDIM", "PRESERVE", "GOSUB",
        "TYPE", "NAMESPACE", "PTR", "ANY", "UNION", "DECLARE", "CONSTRUCTOR", "DESTRUCTOR", "THIS",
        "EXTENDS", "VIRTUAL", "OVERRIDE", "BASE", "PROPERTY", "OPERATOR", "EXTERN", "LIB", "ALIAS",
        "CDECL", "STDCALL", "ZSTRING", "OF",
    };
    return keywords;
}

// LSP CompletionItemKind values (see the spec's own enum) relevant here.
constexpr int kCompletionKindKeyword = 14;
constexpr int kCompletionKindFunction = 3;
constexpr int kCompletionKindClass = 7;
constexpr int kCompletionKindVariable = 6;

void addFromIndex(const ebasic::SemaIndex& index, nlohmann::json& items) {
    for (const auto& [name, info] : index.procedures) {
        (void)info;
        items.push_back({{"label", name}, {"kind", kCompletionKindFunction}});
    }
    for (const auto& [name, info] : index.structs) {
        (void)info;
        items.push_back({{"label", name}, {"kind", kCompletionKindClass}});
    }
    for (const auto& [name, info] : index.symbols) {
        (void)info;
        items.push_back({{"label", name}, {"kind", kCompletionKindVariable}});
    }
}

} // namespace

nlohmann::json completionItems(const ebasic::SemaIndex& index,
                                const std::vector<const ebasic::SemaIndex*>& dependencyIndexes) {
    nlohmann::json items = nlohmann::json::array();
    for (const std::string& kw : reservedKeywords()) {
        items.push_back({{"label", kw}, {"kind", kCompletionKindKeyword}});
    }
    addFromIndex(index, items);
    for (const ebasic::SemaIndex* dep : dependencyIndexes) addFromIndex(*dep, items);
    return items;
}

namespace {

/// True for the statement kinds whose own `name` field is itself a
/// reference to (or the declaration of) a symbol - Assign's simple-target
/// form in particular has no Expr node of its own for its target, unlike
/// every other statement kind, which only ever names things through Exprs.
bool statementNameIsReference(ebasic::StmtKind kind) {
    switch (kind) {
        case ebasic::StmtKind::Dim:
        case ebasic::StmtKind::Const:
        case ebasic::StmtKind::Assign:
        case ebasic::StmtKind::ForNext:
        case ebasic::StmtKind::Goto:
        case ebasic::StmtKind::Label:
        case ebasic::StmtKind::GoSub:
            return true;
        default:
            return false;
    }
}

void walkExpr(const ebasic::Expr* e, const std::string& targetKey, std::vector<ebasic::SourceLoc>& out) {
    if (!e) return;
    if ((e->kind == ebasic::ExprKind::Ident || e->kind == ebasic::ExprKind::Call ||
         e->kind == ebasic::ExprKind::Member) &&
        ebasic::canonicalName(e->stringValue) == targetKey) {
        out.push_back(e->loc);
    }
    walkExpr(e->lhs.get(), targetKey, out);
    walkExpr(e->rhs.get(), targetKey, out);
    for (const auto& arg : e->args) walkExpr(arg.get(), targetKey, out);
}

void walkStmts(const std::vector<ebasic::StmtPtr>& stmts, const std::string& targetKey,
               std::vector<ebasic::SourceLoc>& out);

void walkStmt(const ebasic::Stmt& s, const std::string& targetKey, std::vector<ebasic::SourceLoc>& out) {
    if (statementNameIsReference(s.kind) && ebasic::canonicalName(s.name) == targetKey) {
        out.push_back(s.loc);
    }
    walkExpr(s.expr.get(), targetKey, out);
    for (const auto& a : s.args) walkExpr(a.get(), targetKey, out);
    walkExpr(s.arrayLower.get(), targetKey, out);
    walkExpr(s.arrayUpper.get(), targetKey, out);
    walkExpr(s.index.get(), targetKey, out);
    walkExpr(s.target.get(), targetKey, out);
    for (const auto& c : s.conditions) walkExpr(c.get(), targetKey, out);
    for (const auto& blk : s.blocks) walkStmts(blk, targetKey, out);
    for (const auto& arm : s.cases) {
        for (const auto& m : arm.matches) walkExpr(m.get(), targetKey, out);
        walkStmts(arm.body, targetKey, out);
    }
    walkExpr(s.forEnd.get(), targetKey, out);
    walkExpr(s.forStep.get(), targetKey, out);
    walkStmts(s.body, targetKey, out); // ForNext/WhileWend/DoLoop body, and NamespaceDecl's body
    walkExpr(s.preCond.get(), targetKey, out);
    walkExpr(s.postCond.get(), targetKey, out);
    for (const auto& member : s.enumMembers) walkExpr(member.value.get(), targetKey, out);
    walkStmts(s.methods, targetKey, out); // TYPE method prototypes (declared, not defined, here)
}

void walkStmts(const std::vector<ebasic::StmtPtr>& stmts, const std::string& targetKey,
               std::vector<ebasic::SourceLoc>& out) {
    for (const auto& stmtPtr : stmts) walkStmt(*stmtPtr, targetKey, out);
}

} // namespace

std::vector<ebasic::SourceLoc> findReferences(const ebasic::Module& module, const std::string& targetKey) {
    std::vector<ebasic::SourceLoc> result;
    walkStmts(module.stmts, targetKey, result);
    return result;
}

} // namespace ebasic::lsp
