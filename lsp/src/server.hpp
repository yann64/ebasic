#pragma once

#include "documents.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace ebasic::lsp {

/// The LSP method dispatch loop - reads Content-Length-framed JSON-RPC
/// messages from `in`, dispatches by `method`, writes responses/
/// notifications to `out`. One `Server` per process (real LSP clients spawn
/// a fresh server process per workspace).
class Server {
public:
    /// Runs until `exit` is received or `in` reaches end-of-stream (a client
    /// that disconnects without a clean shutdown/exit sequence). Returns the
    /// process exit code the LSP spec mandates: 0 if `shutdown` was received
    /// before `exit`, 1 otherwise.
    int run(std::istream& in, std::ostream& out);

private:
    void dispatch(const nlohmann::json& msg, std::ostream& out);

    void handleInitialize(const nlohmann::json& id, const nlohmann::json& params, std::ostream& out);
    void handleShutdown(const nlohmann::json& id, std::ostream& out);
    void handleDidOpen(const nlohmann::json& params, std::ostream& out);
    void handleDidChange(const nlohmann::json& params, std::ostream& out);
    void handleDidClose(const nlohmann::json& params);
    void handleDocumentSymbol(const nlohmann::json& id, const nlohmann::json& params, std::ostream& out);
    void handleHover(const nlohmann::json& id, const nlohmann::json& params, std::ostream& out);
    void handleDefinition(const nlohmann::json& id, const nlohmann::json& params, std::ostream& out);
    void handleReferences(const nlohmann::json& id, const nlohmann::json& params, std::ostream& out);

    /// Re-runs the compile pipeline over `uri`'s current text and publishes
    /// one `textDocument/publishDiagnostics` per affected file (the edited
    /// document itself, always - even with zero diagnostics, to clear a
    /// stale set - plus any `#include`d file that has or previously had
    /// diagnostics of its own).
    void publishDiagnostics(const std::string& uri, std::ostream& out);

    void sendResult(std::ostream& out, const nlohmann::json& id, nlohmann::json result);
    void sendError(std::ostream& out, const nlohmann::json& id, int code, const std::string& message);
    void sendNotification(std::ostream& out, const std::string& method, nlohmann::json params);

    DocumentStore documents_;
    bool shutdownReceived_ = false;
    bool exitRequested_ = false;
    /// Keyed by an *edited* document's own URI: the set of file URIs its
    /// last diagnostics run actually published a non-empty array for -
    /// lets the next run know which of those to clear if they're no longer
    /// affected (e.g. an #include that stopped failing).
    std::unordered_map<std::string, std::unordered_set<std::string>> lastDiagnosticUris_;
};

} // namespace ebasic::lsp
