#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace ebasic::lsp {

/// Runs Preprocessor -> Lexer -> Parser -> Sema over `text` as if it were
/// `path`'s current (possibly unsaved) on-disk content - `#include`
/// resolution is relative to `path`'s own directory first, then
/// `includeDirs` (an `ebpm` package's own dependency target directories,
/// from `resolvePackageContext` - empty for a document with no enclosing
/// package), exactly like a real `ebc`/`ebpm build` invocation. Any
/// `#include`d file is read fresh from disk (an unsaved edit to one that's
/// *also* open in the editor isn't picked up until saved - a deliberate
/// simplification, not a bug).
///
/// `missingInterfaces` (canonical dependency names whose own
/// `target/<name>.iface.bas` doesn't exist yet - see
/// `PackageContext::missingInterfaces`) turns a raw "cannot open included
/// file '<name>.iface.bas'" diagnostic into an actionable "run `ebpm
/// build`" hint instead, for any name in this list.
///
/// Returns every diagnostic produced, as LSP `Diagnostic` JSON objects,
/// grouped by the *originating* file's `file://` URI - not necessarily
/// just `path`'s own, since a diagnostic can point inside an `#include`d
/// file. A file with zero diagnostics never gets an entry here; callers
/// publishing `textDocument/publishDiagnostics` are responsible for still
/// sending an empty array to clear a file's previously-published set.
std::unordered_map<std::string, nlohmann::json> computeDiagnostics(
    const std::string& path, const std::string& text, const std::vector<std::string>& includeDirs = {},
    const std::vector<std::string>& missingInterfaces = {});

} // namespace ebasic::lsp
