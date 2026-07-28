#pragma once

#include "documents.hpp"

#include <nlohmann/json.hpp>

#include <iostream>

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
    void handleDidOpen(const nlohmann::json& params);
    void handleDidChange(const nlohmann::json& params);
    void handleDidClose(const nlohmann::json& params);

    void sendResult(std::ostream& out, const nlohmann::json& id, nlohmann::json result);
    void sendError(std::ostream& out, const nlohmann::json& id, int code, const std::string& message);

    DocumentStore documents_;
    bool shutdownReceived_ = false;
    bool exitRequested_ = false;
};

} // namespace ebasic::lsp
