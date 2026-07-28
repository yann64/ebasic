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

std::unordered_map<std::string, nlohmann::json> computeDiagnostics(const std::string& path,
                                                                     const std::string& text) {
    ebasic::DiagnosticEngine diags;
    diags.registerFile(path); // fileId 0, mirroring ebc's own main.cpp

    ebasic::PreprocessResult pre = ebasic::preprocess(text, path, diags, {});
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
                {"message", d->message},
                {"source", "ebasic"},
            });
        }
        result[pathToUri(diags.fileName(fileId))] = std::move(arr);
    }
    return result;
}

} // namespace ebasic::lsp
