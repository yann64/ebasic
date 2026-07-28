#include "server.hpp"
#include "diagnostics.hpp"
#include "rpc.hpp"
#include "symbols.hpp"
#include "uri.hpp"

#include "ast/ast.hpp"
#include "ebasic/version.hpp"

#include <filesystem>

namespace ebasic::lsp {

using json = nlohmann::json;

int Server::run(std::istream& in, std::ostream& out) {
    while (!exitRequested_) {
        std::optional<json> msg = readMessage(in);
        if (!msg) {
            // End-of-stream with no clean exit request - treat the same as
            // shutdown() never having been called (spec-mandated exit code 1).
            return shutdownReceived_ ? 0 : 1;
        }
        if (msg->is_discarded()) {
            continue; // malformed frame - nothing sensible to reply with
        }
        dispatch(*msg, out);
    }
    return shutdownReceived_ ? 0 : 1;
}

void Server::dispatch(const json& msg, std::ostream& out) {
    auto methodIt = msg.find("method");
    if (methodIt == msg.end() || !methodIt->is_string()) return;
    const std::string method = methodIt->get<std::string>();
    const json params = msg.value("params", json::object());
    auto idIt = msg.find("id");
    const bool isRequest = idIt != msg.end();

    // A handler reaches into `params` with .at(...)/.get<T>() freely,
    // trusting a well-formed client - but a malformed or unexpected shape
    // (a missing field, wrong type) must never take the whole server down.
    // One json::exception boundary here covers every handler below rather
    // than repeating a try/catch in each one.
    try {
        if (method == "initialize") {
            if (isRequest) handleInitialize(*idIt, params, out);
        } else if (method == "initialized") {
            // Notification only - nothing to do yet (later slices may kick
            // off workspace-wide ebpm package discovery here).
        } else if (method == "shutdown") {
            if (isRequest) handleShutdown(*idIt, out);
        } else if (method == "exit") {
            exitRequested_ = true;
        } else if (method == "textDocument/didOpen") {
            handleDidOpen(params, out);
        } else if (method == "textDocument/didChange") {
            handleDidChange(params, out);
        } else if (method == "textDocument/didClose") {
            handleDidClose(params);
        } else if (method == "textDocument/documentSymbol") {
            if (isRequest) handleDocumentSymbol(*idIt, params, out);
        } else if (method == "textDocument/hover") {
            if (isRequest) handleHover(*idIt, params, out);
        } else if (method == "textDocument/definition") {
            if (isRequest) handleDefinition(*idIt, params, out);
        } else if (method == "textDocument/references") {
            if (isRequest) handleReferences(*idIt, params, out);
        } else if (method == "workspace/didChangeWatchedFiles") {
            handleDidChangeWatchedFiles(params);
        } else if (isRequest) {
            // MethodNotFound (-32601) - only requests get (and need) a
            // reply; an unrecognized notification is simply ignored, per
            // the spec.
            sendError(out, *idIt, -32601, "method not found: " + method);
        }
    } catch (const json::exception& e) {
        // InvalidParams (-32602). A malformed notification has no `id` to
        // reply to - nothing sensible to do but drop it.
        if (isRequest) sendError(out, *idIt, -32602, std::string("invalid params: ") + e.what());
    }
}

void Server::handleInitialize(const json& id, const json& /*params*/, std::ostream& out) {
    json result = {
        {"capabilities", {
            // Full-document sync only (no incremental ranges) - matches
            // documents.hpp's whole-text replace-on-change model.
            {"textDocumentSync", 1},
            {"documentSymbolProvider", true},
            {"hoverProvider", true},
            {"definitionProvider", true},
            {"referencesProvider", true},
        }},
        {"serverInfo", {
            {"name", "ebasic-lsp"},
            {"version", ebasic::kProjectVersion},
        }},
    };
    sendResult(out, id, std::move(result));
}

void Server::handleShutdown(const json& id, std::ostream& out) {
    shutdownReceived_ = true;
    sendResult(out, id, nullptr);
}

void Server::handleDidOpen(const json& params, std::ostream& out) {
    const json& doc = params.at("textDocument");
    const std::string uri = doc.at("uri").get<std::string>();
    documents_.open(uri, doc.at("text").get<std::string>());
    // Resolve (or re-resolve) this document's enclosing ebpm package now,
    // not lazily inside publishDiagnostics - didOpen is the one point
    // where a real re-resolution (a git fetch, for a git dependency) is
    // worth paying for; didChange must never trigger one per keystroke.
    packageContextFor(uri, /*forceRefresh=*/true);
    publishDiagnostics(uri, out);
}

void Server::handleDidChange(const json& params, std::ostream& out) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    // Full sync (advertised in initialize): a single contentChanges entry
    // holding the entire new text, no `range` field.
    const json& changes = params.at("contentChanges");
    if (!changes.empty()) {
        documents_.update(uri, changes.back().at("text").get<std::string>());
        publishDiagnostics(uri, out);
    }
}

void Server::handleDidClose(const json& params) {
    documents_.close(params.at("textDocument").at("uri").get<std::string>());
}

void Server::publishDiagnostics(const std::string& uri, std::ostream& out) {
    const std::string* text = documents_.find(uri);
    if (!text) return;

    const PackageContext& pkg = packageContextFor(uri, /*forceRefresh=*/false);
    std::unordered_map<std::string, json> byUri =
        computeDiagnostics(uriToPath(uri), *text, pkg.includeDirs, pkg.missingInterfaces);
    if (byUri.find(uri) == byUri.end()) {
        byUri[uri] = json::array(); // always publish for the edited doc, to clear a stale set
    }
    // Anything that had diagnostics last time but doesn't this round (an
    // #include that stopped failing, or the whole file no longer being
    // #included at all) still needs an empty array sent, or the editor
    // would keep showing now-stale markers forever.
    for (const std::string& oldUri : lastDiagnosticUris_[uri]) {
        if (byUri.find(oldUri) == byUri.end()) {
            byUri[oldUri] = json::array();
        }
    }

    std::unordered_set<std::string> newActive;
    for (auto& [fileUri, diagsArr] : byUri) {
        sendNotification(out, "textDocument/publishDiagnostics",
                          {{"uri", fileUri}, {"diagnostics", diagsArr}});
        if (!diagsArr.empty()) newActive.insert(fileUri);
    }
    lastDiagnosticUris_[uri] = std::move(newActive);
}

void Server::handleDocumentSymbol(const json& id, const json& params, std::ostream& out) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const std::string* text = documents_.find(uri);
    if (!text) {
        sendResult(out, id, json::array());
        return;
    }
    const PackageContext& pkg = packageContextFor(uri, /*forceRefresh=*/false);
    auto checked = checkDocument(uriToPath(uri), *text, pkg.includeDirs);
    if (!checked) {
        // A syntax error means no Module at all yet - an empty outline,
        // not an error reply (a request failure would be more disruptive
        // than useful for something this cosmetic).
        sendResult(out, id, json::array());
        return;
    }
    sendResult(out, id, documentSymbols(checked->module));
}

void Server::handleHover(const json& id, const json& params, std::ostream& out) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const json& position = params.at("position");
    const int line = position.at("line").get<int>();
    const int character = position.at("character").get<int>();

    const std::string* text = documents_.find(uri);
    if (!text) {
        sendResult(out, id, nullptr);
        return;
    }
    std::optional<std::string> word = identifierAt(*text, line, character);
    if (!word) {
        sendResult(out, id, nullptr);
        return;
    }
    const PackageContext& pkg = packageContextFor(uri, /*forceRefresh=*/false);
    auto checked = checkDocument(uriToPath(uri), *text, pkg.includeDirs);
    if (!checked) {
        sendResult(out, id, nullptr);
        return;
    }
    std::optional<json> result = hoverFor(checked->index, *word);
    if (!result) {
        // Not found in this document's own symbols - try each of the
        // package's own dependencies' parsed interfaces next, in
        // resolution order, before giving up.
        for (const auto& [depName, dep] : pkg.dependencies) {
            (void)depName;
            result = hoverFor(dep.index, *word);
            if (result) break;
        }
    }
    sendResult(out, id, result ? *result : json(nullptr));
}

void Server::handleDefinition(const json& id, const json& params, std::ostream& out) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const json& position = params.at("position");
    const int line = position.at("line").get<int>();
    const int character = position.at("character").get<int>();

    const std::string* text = documents_.find(uri);
    if (!text) {
        sendResult(out, id, json::array());
        return;
    }
    std::optional<std::string> word = identifierAt(*text, line, character);
    if (!word) {
        sendResult(out, id, json::array());
        return;
    }
    const PackageContext& pkg = packageContextFor(uri, /*forceRefresh=*/false);
    auto checked = checkDocument(uriToPath(uri), *text, pkg.includeDirs);
    if (!checked) {
        sendResult(out, id, json::array());
        return;
    }
    std::optional<ebasic::SourceLoc> loc = declLocFor(checked->index, *word);
    if (loc) {
        json location = {{"uri", pathToUri(checked->diags.fileName(loc->fileId))}, {"range", pointRange(*loc)}};
        sendResult(out, id, location);
        return;
    }
    // Not found in this document's own symbols - try each of the
    // package's own dependencies' parsed interfaces next: a
    // go-to-definition landing in one lands in its generated interface
    // file (its real, on-disk contract), the same UX convention many
    // language servers use for pre-built/vendored dependencies.
    for (const auto& [depName, dep] : pkg.dependencies) {
        (void)depName;
        std::optional<ebasic::SourceLoc> depLoc = declLocFor(dep.index, *word);
        if (depLoc) {
            // .iface.bas is always flat (auto-generated, no #include of
            // its own), so its only fileId is the file itself.
            json location = {{"uri", pathToUri(dep.path)}, {"range", pointRange(*depLoc)}};
            sendResult(out, id, location);
            return;
        }
    }
    sendResult(out, id, json::array());
}

void Server::handleReferences(const json& id, const json& params, std::ostream& out) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const json& position = params.at("position");
    const int line = position.at("line").get<int>();
    const int character = position.at("character").get<int>();
    const bool includeDeclaration = params.value("context", json::object()).value("includeDeclaration", true);

    const std::string* text = documents_.find(uri);
    if (!text) {
        sendResult(out, id, json::array());
        return;
    }
    std::optional<std::string> word = identifierAt(*text, line, character);
    if (!word) {
        sendResult(out, id, json::array());
        return;
    }
    const PackageContext& pkg = packageContextFor(uri, /*forceRefresh=*/false);
    auto checked = checkDocument(uriToPath(uri), *text, pkg.includeDirs);
    if (!checked) {
        sendResult(out, id, json::array());
        return;
    }
    const std::string key = ebasic::canonicalName(*word);
    std::vector<ebasic::SourceLoc> locs = findReferences(checked->module, key);
    std::optional<ebasic::SourceLoc> declLoc = declLocFor(checked->index, *word);

    // findReferences already reports the declaration site for a variable-
    // like symbol (Dim/Const/ForNext's own `name` is itself a reference in
    // the walker's own terms) but never for a SUB/FUNCTION/TYPE (whose
    // declaring Stmt's `name` isn't a reference site at all) - reconcile
    // both against includeDeclaration explicitly rather than relying on
    // the walker to have gotten it right for every symbol kind.
    auto sameLoc = [](const ebasic::SourceLoc& a, const ebasic::SourceLoc& b) {
        return a.fileId == b.fileId && a.line == b.line && a.column == b.column;
    };
    if (declLoc) {
        bool alreadyPresent = false;
        for (auto it = locs.begin(); it != locs.end();) {
            if (sameLoc(*it, *declLoc)) {
                if (!includeDeclaration) {
                    it = locs.erase(it);
                    continue;
                }
                alreadyPresent = true;
            }
            ++it;
        }
        if (includeDeclaration && !alreadyPresent) locs.push_back(*declLoc);
    }

    json result = json::array();
    for (const ebasic::SourceLoc& loc : locs) {
        result.push_back({{"uri", pathToUri(checked->diags.fileName(loc.fileId))}, {"range", pointRange(loc)}});
    }
    sendResult(out, id, result);
}

void Server::handleDidChangeWatchedFiles(const json& params) {
    // Each changed file's own directory might be (or be under) a package
    // root already cached - rather than recomputing every affected
    // package's own root exactly, just drop any cache entry whose root is
    // an ancestor of the changed path, so the next packageContextFor call
    // for a document in that package re-resolves instead of reusing stale
    // data. NOTE: this handler only ever fires if the client actually
    // sends this notification - this server doesn't yet dynamically
    // register interest in it (client/registerCapability), so most
    // clients won't send it unprompted; reopening the document (or
    // restarting the server) is the reliable fallback today. See
    // docs/guide/lsp.md.
    for (const auto& change : params.value("changes", json::array())) {
        std::string changedPath = uriToPath(change.value("uri", std::string()));
        for (auto it = packageCache_.begin(); it != packageCache_.end();) {
            const std::string& root = it->first;
            if (changedPath.compare(0, root.size(), root) == 0) {
                it = packageCache_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

const PackageContext& Server::packageContextFor(const std::string& uri, bool forceRefresh) {
    static const PackageContext empty;
    std::string dir = std::filesystem::path(uriToPath(uri)).parent_path().string();
    std::optional<std::string> root = findPackageRoot(dir);
    if (!root) return empty;
    if (forceRefresh || packageCache_.find(*root) == packageCache_.end()) {
        packageCache_[*root] = resolvePackageContext(*root);
    }
    return packageCache_[*root];
}

void Server::sendResult(std::ostream& out, const json& id, json result) {
    json response = {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
    writeMessage(out, response);
}

void Server::sendError(std::ostream& out, const json& id, int code, const std::string& message) {
    json response = {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    writeMessage(out, response);
}

void Server::sendNotification(std::ostream& out, const std::string& method, json params) {
    json notification = {{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}};
    writeMessage(out, notification);
}

} // namespace ebasic::lsp
