#include "uri.hpp"

#include <cctype>

namespace ebasic::lsp {

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percentDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hexValue(s[i + 1]);
            int lo = hexValue(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

bool isUnreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
}

std::string percentEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (isUnreserved(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

/// True if `path` starts with a Windows drive letter (`C:...`, `C:/...`).
bool startsWithDriveLetter(const std::string& path) {
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':';
}

} // namespace

std::string uriToPath(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.compare(0, prefix.size(), prefix) != 0) return uri;
    std::string rest = percentDecode(uri.substr(prefix.size()));
    // RFC 8089's Windows form: "file:///C:/..." -> rest == "/C:/...".
    if (rest.size() >= 3 && rest[0] == '/' && startsWithDriveLetter(rest.substr(1))) {
        rest.erase(0, 1);
    }
    return rest;
}

std::string pathToUri(const std::string& path) {
    std::string normalized = path;
    for (char& c : normalized) {
        if (c == '\\') c = '/';
    }
    std::string prefix = startsWithDriveLetter(normalized) ? "file:///" : "file://";
    return prefix + percentEncode(normalized);
}

} // namespace ebasic::lsp
