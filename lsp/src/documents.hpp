#pragma once

#include <string>
#include <unordered_map>

namespace ebasic::lsp {

/// The server's in-memory mirror of every currently-open document, keyed by
/// its `file://...` URI. The editor is the source of truth - `didOpen`
/// sends the full initial text, `didChange` (under `Full` sync) the full
/// replacement text - this class just tracks the latest version the server
/// has seen, for the analysis passes (diagnostics, hover, ...) later slices
/// add.
class DocumentStore {
public:
    void open(const std::string& uri, std::string text);
    void update(const std::string& uri, std::string text);
    void close(const std::string& uri);

    /// nullptr if `uri` isn't currently open (e.g. a stray notification
    /// after didClose, or referring to a file never opened at all).
    const std::string* find(const std::string& uri) const;

private:
    std::unordered_map<std::string, std::string> documents_;
};

} // namespace ebasic::lsp
