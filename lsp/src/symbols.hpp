#pragma once

#include "ast/ast.hpp"
#include "sema/sema.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ebasic::lsp {

/// The result of successfully parsing (and, when possible, checking)
/// `text` - kept together since every AST node `index` might point back
/// into (declLoc, param/field types, ...) lives inside `module`. `diags`
/// is kept too (not just its diagnostics, which are discarded) so a
/// caller can resolve a declLoc's fileId back to a real path via
/// `diags.fileName(fileId)` - needed for go-to-definition landing in a
/// different file than the one being edited (an `#include`).
struct CheckedDocument {
    ebasic::Module module;
    ebasic::SemaIndex index;
    ebasic::DiagnosticEngine diags;
};

/// Runs Preprocessor -> Lexer -> Parser -> Sema over `text` as if it were
/// `path`'s current content (mirrors computeDiagnostics's own pipeline,
/// including `includeDirs` - an `ebpm` package's own dependency target
/// directories, from `resolvePackageContext`; empty for a document with no
/// enclosing package). Returns nullopt only if Preprocessor/Lexer/Parser
/// themselves failed (no usable Module at all) - if Sema finds body-level
/// errors, this still returns the Module and whatever SemaIndex Sema
/// built, since collectProcedures/collectTypes (module-level signatures)
/// always run to completion before any body is checked, so real symbol
/// information is available even for a document with in-progress edits
/// elsewhere in it.
std::optional<CheckedDocument> checkDocument(const std::string& path, const std::string& text,
                                              const std::vector<std::string>& includeDirs = {});

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

/// Where `rawName` (any casing) was declared, for `textDocument/definition`
/// - same lookup order as hoverFor (procedure, then TYPE/UNION, then
/// variable/CONST). nullopt if `rawName` isn't any known symbol.
std::optional<ebasic::SourceLoc> declLocFor(const ebasic::SemaIndex& index, const std::string& rawName);

/// An LSP `Range` covering exactly one character starting at `loc` - the
/// closest honest approximation available (eBasic's AST tracks a single
/// point per node, never a start/end span). Shared by documentSymbols,
/// diagnostics.cpp, and definition/references so every LSP-facing range
/// this server produces is built the same way.
nlohmann::json pointRange(const ebasic::SourceLoc& loc);

/// Every `Ident`/`Call`/`Member` expression (plus each `Dim`/`Const`/
/// `Assign`/`ForNext`/`Goto`/`Label`/`GoSub` statement's own `name`) in
/// `module` whose canonical form matches `targetKey` (already
/// canonicalized) - `textDocument/references`' result, single-
/// compilation-unit scope (an `#include`d file is already part of the same
/// `Module` after preprocessing, so this covers it; a *dependency*
/// package's own separate compilation is LSP-5 scope).
std::vector<ebasic::SourceLoc> findReferences(const ebasic::Module& module, const std::string& targetKey);

} // namespace ebasic::lsp
