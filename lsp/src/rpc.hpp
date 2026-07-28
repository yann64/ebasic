#pragma once

#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>

namespace ebasic::lsp {

/// Reads one Content-Length-framed JSON-RPC message from `in` (LSP's own
/// transport: one or more `Header: value` lines, a blank line, then exactly
/// `Content-Length` bytes of UTF-8 JSON). Tolerates a bare `\n` line ending
/// as well as the spec-mandated `\r\n`. Returns nullopt at end-of-stream
/// (the client closed its side without a clean shutdown/exit sequence) -
/// callers should treat that the same as a client-initiated exit. A
/// malformed body still returns a value; check `.is_discarded()`.
std::optional<nlohmann::json> readMessage(std::istream& in);

/// Writes one Content-Length-framed JSON-RPC message to `out` and flushes
/// immediately - the client reads this synchronously and blocks until the
/// full frame arrives.
void writeMessage(std::ostream& out, const nlohmann::json& message);

} // namespace ebasic::lsp
