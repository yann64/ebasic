#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

namespace ebasic::lsp {

/// Runs Preprocessor -> Lexer -> Parser -> Sema over `text` as if it were
/// `path`'s current (possibly unsaved) on-disk content - `#include`
/// resolution is relative to `path`'s own directory, exactly like a real
/// `ebc` invocation. Any `#include`d file is read fresh from disk (an
/// unsaved edit to one that's *also* open in the editor isn't picked up
/// until saved - a deliberate simplification, not a bug).
///
/// Returns every diagnostic produced, as LSP `Diagnostic` JSON objects,
/// grouped by the *originating* file's `file://` URI - not necessarily
/// just `path`'s own, since a diagnostic can point inside an `#include`d
/// file. A file with zero diagnostics never gets an entry here; callers
/// publishing `textDocument/publishDiagnostics` are responsible for still
/// sending an empty array to clear a file's previously-published set.
std::unordered_map<std::string, nlohmann::json> computeDiagnostics(const std::string& path,
                                                                     const std::string& text);

} // namespace ebasic::lsp
