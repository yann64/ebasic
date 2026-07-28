#pragma once

#include "ast/ast.hpp"
#include "sema/sema.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace ebasic::lsp {

/// The result of successfully parsing (and, when possible, checking)
/// `text` - kept together since every AST node `index` might point back
/// into (declLoc, param/field types, ...) lives inside `module`.
struct CheckedDocument {
    ebasic::Module module;
    ebasic::SemaIndex index;
};

/// Runs Preprocessor -> Lexer -> Parser -> Sema over `text` as if it were
/// `path`'s current content (mirrors computeDiagnostics's own pipeline).
/// Returns nullopt only if Preprocessor/Lexer/Parser themselves failed (no
/// usable Module at all) - if Sema finds body-level errors, this still
/// returns the Module and whatever SemaIndex Sema built, since
/// collectProcedures/collectTypes (module-level signatures) always run to
/// completion before any body is checked, so real symbol information is
/// available even for a document with in-progress edits elsewhere in it.
std::optional<CheckedDocument> checkDocument(const std::string& path, const std::string& text);

/// Finds the identifier token, if any, spanning 0-based `line`/`character`
/// in `text` - re-lexes just that one line in isolation. eBasic's AST
/// tracks a single point per node, not a start/end span, so there's no
/// range to test a position against directly; re-lexing the requested line
/// gives real token boundaries without needing that.
std::optional<std::string> identifierAt(const std::string& text, int line, int character);

/// `textDocument/documentSymbol` result: one entry per top-level
/// TYPE/UNION/CONST/ENUM/NAMESPACE/free SUB/FUNCTION, in source order -
/// mirrors docgen/src/render.cpp's own collectSections walk (same
/// declaration kinds, same "TYPE method definitions don't count" rule),
/// producing LSP `DocumentSymbol` JSON instead of a Markdown section.
nlohmann::json documentSymbols(const ebasic::Module& module);

/// `textDocument/hover` result for the symbol spelled `rawName` at the
/// hover site (its original casing, for display - BASIC identifiers are
/// case-insensitive, so this is looked up in `index` by its canonical
/// form), checked in procedure, then TYPE/UNION, then variable/CONST
/// order. nullopt if `rawName` isn't any known symbol (e.g. a keyword, or
/// a typo).
std::optional<nlohmann::json> hoverFor(const ebasic::SemaIndex& index, const std::string& rawName);

} // namespace ebasic::lsp
