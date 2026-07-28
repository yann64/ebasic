#include "diagnostics.hpp"
#include "uri.hpp"

#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "sema/sema.hpp"

#include <map>
#include <vector>

namespace ebasic::lsp {

namespace {

/// Rewrites a raw "cannot open included file '<name>.iface.bas'" message
/// (preprocessor.cpp's own wording) into an actionable hint when `<name>`
/// is a known dependency that just hasn't been built yet - `message`
/// unchanged otherwise (a genuinely missing/misspelled #include target, or
/// one naming a dependency this package doesn't even have, still gets the
/// original, honest error).
std::string annotateMissingInterface(const std::string& message,
                                      const std::vector<std::string>& missingInterfaces) {
    const std::string prefix = "cannot open included file '";
    if (message.compare(0, prefix.size(), prefix) != 0 || message.back() != '\'') return message;
    std::string quoted = message.substr(prefix.size(), message.size() - prefix.size() - 1);
    const std::string suffix = ".iface.bas";
    if (quoted.size() <= suffix.size() || quoted.compare(quoted.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return message;
    }
    std::string depName = quoted.substr(0, quoted.size() - suffix.size());
    for (const std::string& missing : missingInterfaces) {
        if (missing == depName) {
            return message + " - dependency '" + depName + "' hasn't been built yet; run `ebpm build`";
        }
    }
    return message;
}

} // namespace

std::unordered_map<std::string, nlohmann::json> computeDiagnostics(
    const std::string& path, const std::string& text, const std::vector<std::string>& includeDirs,
    const std::vector<std::string>& missingInterfaces) {
    ebasic::DiagnosticEngine diags;
    diags.registerFile(path); // fileId 0, mirroring ebc's own main.cpp

    ebasic::PreprocessResult pre = ebasic::preprocess(text, path, diags, includeDirs);
    if (!diags.hasErrors()) {
        ebasic::Lexer lexer(pre.source, pre.lineMap, diags);
        std::vector<ebasic::Token> tokens = lexer.tokenize();
        if (!diags.hasErrors()) {
            ebasic::Parser parser(std::move(tokens), diags);
            ebasic::Module module = parser.parseModule();
            if (!diags.hasErrors()) {
                ebasic::Sema sema(diags);
                sema.check(module);
            }
        }
    }

    // Group by fileId first (stable iteration order isn't needed - each
    // group becomes its own independent publishDiagnostics notification).
    std::map<int, std::vector<const ebasic::Diagnostic*>> byFileId;
    for (const ebasic::Diagnostic& d : diags.diagnostics()) {
        byFileId[d.loc.fileId].push_back(&d);
    }

    std::unordered_map<std::string, nlohmann::json> result;
    for (const auto& [fileId, ds] : byFileId) {
        nlohmann::json arr = nlohmann::json::array();
        for (const ebasic::Diagnostic* d : ds) {
            // SourceLoc is a single point (no end position tracked
            // anywhere in the AST/lexer) - a one-character-wide range is
            // the closest honest approximation an editor can still
            // usefully underline. LSP positions are 0-based; SourceLoc's
            // line/column are 1-based.
            arr.push_back({
                {"range", {
                    {"start", {{"line", d->loc.line - 1}, {"character", d->loc.column - 1}}},
                    {"end", {{"line", d->loc.line - 1}, {"character", d->loc.column}}},
                }},
                {"severity", d->severity == ebasic::Severity::Error ? 1 : 2},
                {"message", annotateMissingInterface(d->message, missingInterfaces)},
                {"source", "ebasic"},
            });
        }
        result[pathToUri(diags.fileName(fileId))] = std::move(arr);
    }
    return result;
}

} // namespace ebasic::lsp
