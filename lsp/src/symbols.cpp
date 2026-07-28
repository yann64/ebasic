#include "symbols.hpp"

#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include <sstream>
#include <vector>

namespace ebasic::lsp {

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

nlohmann::json pointRange(const ebasic::SourceLoc& loc) {
    // One character wide - the closest honest approximation available:
    // eBasic's AST tracks a single point per node, never a start/end span.
    return {
        {"start", {{"line", loc.line - 1}, {"character", loc.column - 1}}},
        {"end", {{"line", loc.line - 1}, {"character", loc.column}}},
    };
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

std::optional<CheckedDocument> checkDocument(const std::string& path, const std::string& text) {
    ebasic::DiagnosticEngine diags;
    diags.registerFile(path);

    ebasic::PreprocessResult pre = ebasic::preprocess(text, path, diags, {});
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
    return CheckedDocument{std::move(module), sema.index()};
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

} // namespace ebasic::lsp
