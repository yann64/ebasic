#include "render.hpp"

#include <sstream>
#include <vector>

namespace docgen {

namespace {

using ebasic::Stmt;
using ebasic::StmtKind;
using ebasic::Type;
using ebasic::TypeKind;

/// Renders `type` back into BASIC source syntax - docgen's own small copy
/// of Codegen::basicTypeName's logic (M5a). Not shared: Codegen's version
/// is a private static method on a class docgen has no other reason to
/// link against, and the whole function is a handful of lines either way.
std::string basicTypeName(const Type& type) {
    switch (type.kind) {
        case TypeKind::Byte: return "BYTE";
        case TypeKind::UByte: return "UBYTE";
        case TypeKind::Short: return "SHORT";
        case TypeKind::UShort: return "USHORT";
        case TypeKind::Integer: return "INTEGER";
        case TypeKind::Long: return "LONG";
        case TypeKind::UInteger: return "UINTEGER";
        case TypeKind::LongInt: return "LONGINT";
        case TypeKind::ULongInt: return "ULONGINT";
        case TypeKind::Single: return "SINGLE";
        case TypeKind::Double: return "DOUBLE";
        case TypeKind::Boolean: return "BOOLEAN";
        case TypeKind::StringT: return "STRING";
        case TypeKind::ZStringT: return "ZSTRING";
        case TypeKind::UserDefined: return type.typeName;
        case TypeKind::Pointer:
            return (type.pointee ? basicTypeName(*type.pointee) : std::string("ANY")) + " PTR";
        case TypeKind::Unknown: return "INTEGER"; // unreachable for a resolved declaration
    }
    return "INTEGER";
}

/// Re-renders a TYPE/UNION's own header + field list as BASIC source text,
/// for display alongside its doc comment - the doc page shows the real
/// declaration shape, not just its name.
std::string renderTypeSignature(const Stmt& stmt) {
    std::ostringstream out;
    bool isUnion = stmt.kind == StmtKind::UnionDecl;
    out << (isUnion ? "UNION " : "TYPE ") << stmt.name;
    if (!stmt.baseTypeName.empty()) out << " EXTENDS " << stmt.baseTypeName;
    out << "\n";
    for (const auto& field : stmt.fields) {
        out << "    " << field.name << " AS " << basicTypeName(field.type) << "\n";
    }
    out << (isUnion ? "END UNION" : "END TYPE");
    return out.str();
}

/// Same as renderTypeSignature, for a single CONST declaration - shows its
/// declared type, not its initializer value.
std::string renderConstSignature(const Stmt& stmt) {
    return "CONST " + stmt.name + " AS " + basicTypeName(stmt.declaredType);
}

/// Member values are deliberately not shown - resolving an ENUM member's
/// auto-incremented/expression value is Sema's job (Stmt::enumMembers'
/// `resolvedValue`), and docgen never runs Sema (only Preprocessor/Lexer/
/// Parser - see the M7 plan), so it would only ever be able to show the
/// unresolved default for most members. Documented as a deliberate
/// simplification rather than showing misleadingly-wrong values.
std::string renderEnumSignature(const Stmt& stmt) {
    std::ostringstream out;
    out << "ENUM " << stmt.name << "\n";
    for (const auto& member : stmt.enumMembers) {
        out << "    " << member.name << "\n";
    }
    out << "END ENUM";
    return out.str();
}

std::string renderNamespaceSignature(const Stmt& stmt) { return "NAMESPACE " + stmt.name; }

std::string renderProcSignature(const Stmt& stmt) {
    std::ostringstream out;
    bool isFunction = stmt.kind == StmtKind::FunctionDecl;
    out << (isFunction ? "FUNCTION " : "SUB ") << stmt.name << "(";
    for (size_t i = 0; i < stmt.params.size(); ++i) {
        if (i > 0) out << ", ";
        const auto& p = stmt.params[i];
        out << (p.byRef ? "BYREF " : "BYVAL ") << p.name << " AS " << basicTypeName(p.type);
    }
    out << ")";
    if (isFunction) out << " AS " << basicTypeName(stmt.declaredType);
    return out.str();
}

/// True for a top-level SUB/FUNCTION that's a free procedure, not a TYPE
/// method's out-of-line definition (`ownerType` non-empty) - TYPE members
/// are out of scope for v1 (see the M7 plan) even though the parser
/// happens to capture a doc comment placed above one just like any other
/// top-level statement (Parser::isDocumentableKind only checks `kind`, not
/// `ownerType`) - docgen is the layer that decides not to surface it.
bool isFreeProc(const Stmt& stmt) {
    return (stmt.kind == StmtKind::SubDecl || stmt.kind == StmtKind::FunctionDecl) &&
           stmt.ownerType.empty();
}

struct Section {
    std::string heading;
    std::vector<const Stmt*> entries;
    std::string (*signatureFn)(const Stmt&);
};

std::vector<Section> collectSections(const ebasic::Module& module) {
    std::vector<Section> sections = {
        {"Types", {}, renderTypeSignature},
        {"Constants", {}, renderConstSignature},
        {"Enums", {}, renderEnumSignature},
        {"Namespaces", {}, renderNamespaceSignature},
        {"Functions/Subs", {}, renderProcSignature},
    };
    for (const auto& stmtPtr : module.stmts) {
        const Stmt& stmt = *stmtPtr;
        if (stmt.kind == StmtKind::TypeDecl || stmt.kind == StmtKind::UnionDecl) {
            sections[0].entries.push_back(&stmt);
        } else if (stmt.kind == StmtKind::Const) {
            sections[1].entries.push_back(&stmt);
        } else if (stmt.kind == StmtKind::Enum) {
            sections[2].entries.push_back(&stmt);
        } else if (stmt.kind == StmtKind::NamespaceDecl) {
            sections[3].entries.push_back(&stmt);
        } else if (isFreeProc(stmt)) {
            sections[4].entries.push_back(&stmt);
        }
    }
    return sections;
}

std::string htmlEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            default: r.push_back(c);
        }
    }
    return r;
}

/// Splits `text` into blank-line-separated paragraphs, each wrapped in
/// <p>...</p> with lines joined by a single space and HTML-escaped - the
/// entire extent of this renderer's "Markdown" support (see render.hpp).
std::string renderProseAsHtml(const std::string& text) {
    std::ostringstream out;
    std::string para;
    auto flush = [&]() {
        if (!para.empty()) {
            out << "<p>" << htmlEscape(para) << "</p>\n";
            para.clear();
        }
    };
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            flush();
            continue;
        }
        if (!para.empty()) para += " ";
        para += line;
    }
    flush();
    return out.str();
}

} // namespace

std::string renderMarkdown(const ebasic::Module& module, const std::string& title) {
    std::ostringstream out;
    out << "# " << title << "\n\n";
    for (const Section& section : collectSections(module)) {
        if (section.entries.empty()) continue;
        out << "## " << section.heading << "\n\n";
        for (const Stmt* stmt : section.entries) {
            out << "### " << stmt->name << "\n\n";
            out << (stmt->docComment.empty() ? "*(undocumented)*" : stmt->docComment) << "\n\n";
            out << "```\n" << section.signatureFn(*stmt) << "\n```\n\n";
        }
    }
    return out.str();
}

std::string renderHtml(const ebasic::Module& module, const std::string& title) {
    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>" << htmlEscape(title)
        << "</title>\n";
    out << "<style>body{font-family:sans-serif;max-width:48rem;margin:2rem auto;padding:0 1rem;"
           "line-height:1.5}h2{border-bottom:1px solid #ccc;padding-bottom:.25rem;margin-top:2rem}"
           "pre{background:#f4f4f4;padding:.75rem;overflow-x:auto}</style>\n";
    out << "</head><body>\n<h1>" << htmlEscape(title) << "</h1>\n";
    for (const Section& section : collectSections(module)) {
        if (section.entries.empty()) continue;
        out << "<h2>" << htmlEscape(section.heading) << "</h2>\n";
        for (const Stmt* stmt : section.entries) {
            out << "<h3>" << htmlEscape(stmt->name) << "</h3>\n";
            if (stmt->docComment.empty()) {
                out << "<p><em>(undocumented)</em></p>\n";
            } else {
                out << renderProseAsHtml(stmt->docComment);
            }
            out << "<pre><code>" << htmlEscape(section.signatureFn(*stmt)) << "</code></pre>\n";
        }
    }
    out << "</body></html>\n";
    return out.str();
}

} // namespace docgen
