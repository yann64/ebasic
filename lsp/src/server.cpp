#include "server.hpp"
#include "rpc.hpp"

#include "ebasic/version.hpp"

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

    if (method == "initialize") {
        if (isRequest) handleInitialize(*idIt, params, out);
    } else if (method == "initialized") {
        // Notification only - nothing to do yet (later slices may kick off
        // workspace-wide ebpm package discovery here).
    } else if (method == "shutdown") {
        if (isRequest) handleShutdown(*idIt, out);
    } else if (method == "exit") {
        exitRequested_ = true;
    } else if (method == "textDocument/didOpen") {
        handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
        handleDidChange(params);
    } else if (method == "textDocument/didClose") {
        handleDidClose(params);
    } else if (isRequest) {
        // MethodNotFound (-32601) - only requests get (and need) a reply;
        // an unrecognized notification is simply ignored, per the spec.
        sendError(out, *idIt, -32601, "method not found: " + method);
    }
}

void Server::handleInitialize(const json& id, const json& /*params*/, std::ostream& out) {
    json result = {
        {"capabilities", {
            // Full-document sync only (no incremental ranges) - matches
            // documents.hpp's whole-text replace-on-change model.
            {"textDocumentSync", 1},
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

void Server::handleDidOpen(const json& params) {
    const json& doc = params.at("textDocument");
    documents_.open(doc.at("uri").get<std::string>(), doc.at("text").get<std::string>());
}

void Server::handleDidChange(const json& params) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    // Full sync (advertised in initialize): a single contentChanges entry
    // holding the entire new text, no `range` field.
    const json& changes = params.at("contentChanges");
    if (!changes.empty()) {
        documents_.update(uri, changes.back().at("text").get<std::string>());
    }
}

void Server::handleDidClose(const json& params) {
    documents_.close(params.at("textDocument").at("uri").get<std::string>());
}

void Server::sendResult(std::ostream& out, const json& id, json result) {
    json response = {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
    writeMessage(out, response);
}

void Server::sendError(std::ostream& out, const json& id, int code, const std::string& message) {
    json response = {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    writeMessage(out, response);
}

} // namespace ebasic::lsp
