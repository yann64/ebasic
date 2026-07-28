#include "rpc.hpp"

namespace ebasic::lsp {

namespace {

/// Reads one header line, stripping a trailing `\r` if present. Returns
/// false only at end-of-stream before any character of this line was read
/// (a genuine "nothing left to read" - not a legitimately empty line, which
/// still returns true with `outLine` cleared).
bool readHeaderLine(std::istream& in, std::string& outLine) {
    outLine.clear();
    int c = in.get();
    if (c == EOF) return false;
    while (c != EOF && c != '\n') {
        if (c != '\r') outLine.push_back(static_cast<char>(c));
        c = in.get();
    }
    return true;
}

} // namespace

std::optional<nlohmann::json> readMessage(std::istream& in) {
    size_t contentLength = 0;
    bool haveLength = false;
    std::string line;
    while (readHeaderLine(in, line)) {
        if (line.empty()) break; // blank line ends the header section
        const std::string key = "Content-Length:";
        if (line.compare(0, key.size(), key) == 0) {
            size_t start = key.size();
            while (start < line.size() && line[start] == ' ') ++start;
            contentLength = static_cast<size_t>(std::stoul(line.substr(start)));
            haveLength = true;
        }
        // Content-Type (if sent) is ignored - this server only ever speaks
        // the default UTF-8 JSON body.
    }
    if (!haveLength) return std::nullopt; // stream ended before/without a real header

    std::string body(contentLength, '\0');
    in.read(&body[0], static_cast<std::streamsize>(contentLength));
    if (static_cast<size_t>(in.gcount()) != contentLength) return std::nullopt;
    return nlohmann::json::parse(body, nullptr, false);
}

void writeMessage(std::ostream& out, const nlohmann::json& message) {
    std::string body = message.dump();
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out.flush();
}

} // namespace ebasic::lsp
