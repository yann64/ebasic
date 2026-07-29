#include "semver.hpp"

#include <cctype>

namespace ebpm {

namespace {

/// Rejects an empty string, anything non-digit, and a leading zero on a
/// value with more than one digit (matching real SemVer's own numeric
/// identifier rule - "01" is not a valid component).
bool parseNonNegativeInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    if (s.size() > 1 && s[0] == '0') return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = std::stoi(s);
    return true;
}

/// Splits on '.', preserving empty segments (e.g. "1..2" or a trailing
/// dot) so a malformed input is caught by the caller's own int-parsing
/// rather than silently swallowed here.
std::vector<std::string> splitDots(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find('.', start);
        if (pos == std::string::npos) pos = s.size();
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
        if (pos == s.size()) break;
    }
    return parts;
}

/// Caret's upper bound: increment the first nonzero component scanning
/// major -> minor -> patch (zeroing everything to its right); if every
/// *given* component is zero, the bound is one past the last given
/// component (`^0.0.3` -> `<0.0.4`, `^0.0` -> `<0.1.0`, `^0` -> `<1.0.0`).
SemVer caretUpperBound(const VersionReq& req) {
    if (req.major != 0) return SemVer{req.major + 1, 0, 0};
    if (req.precision >= 2 && req.minor != 0) return SemVer{0, req.minor + 1, 0};
    if (req.precision >= 3) return SemVer{0, req.minor, req.patch + 1};
    if (req.precision == 2) return SemVer{0, req.minor + 1, 0}; // req.minor == 0 here
    return SemVer{1, 0, 0}; // precision == 1, major == 0 ("^0")
}

/// Tilde's upper bound: patch-level looseness if a minor was given at all,
/// else minor-level (same as Caret with no minor given).
SemVer tildeUpperBound(const VersionReq& req) {
    if (req.precision >= 2) return SemVer{req.major, req.minor + 1, 0};
    return SemVer{req.major + 1, 0, 0};
}

SemVer lowerBound(const VersionReq& req) { return SemVer{req.major, req.minor, req.patch}; }

} // namespace

bool operator==(const SemVer& a, const SemVer& b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

bool operator<(const SemVer& a, const SemVer& b) {
    if (a.major != b.major) return a.major < b.major;
    if (a.minor != b.minor) return a.minor < b.minor;
    return a.patch < b.patch;
}

bool parseSemVer(const std::string& text, SemVer& out, std::string& err) {
    std::vector<std::string> parts = splitDots(text);
    if (parts.size() != 3 || !parseNonNegativeInt(parts[0], out.major) ||
        !parseNonNegativeInt(parts[1], out.minor) || !parseNonNegativeInt(parts[2], out.patch)) {
        err = "'" + text + "' is not a valid version (expected MAJOR.MINOR.PATCH, each a "
                            "non-negative integer with no leading zeros)";
        return false;
    }
    return true;
}

std::string toString(const SemVer& v) {
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
}

bool parseVersionReq(const std::string& text, VersionReq& out, std::string& err) {
    if (text.empty()) {
        err = "empty version requirement";
        return false;
    }
    std::string body = text;
    VersionReq::Op op = VersionReq::Op::Caret;
    if (body[0] == '^' || body[0] == '~' || body[0] == '=') {
        op = body[0] == '^' ? VersionReq::Op::Caret
                             : (body[0] == '~' ? VersionReq::Op::Tilde : VersionReq::Op::Exact);
        body = body.substr(1);
    }
    std::vector<std::string> parts = splitDots(body);
    if (parts.empty() || parts.size() > 3) {
        err = "'" + text + "' is not a valid version requirement";
        return false;
    }
    if (op == VersionReq::Op::Exact && parts.size() != 3) {
        err = "'" + text + "' - an exact ('=') requirement must give a full MAJOR.MINOR.PATCH "
                            "version, not a partial one";
        return false;
    }
    int values[3] = {0, 0, 0};
    for (size_t i = 0; i < parts.size(); ++i) {
        if (!parseNonNegativeInt(parts[i], values[i])) {
            err = "'" + text + "' is not a valid version requirement";
            return false;
        }
    }
    out.op = op;
    out.major = values[0];
    out.minor = values[1];
    out.patch = values[2];
    out.precision = static_cast<int>(parts.size());
    return true;
}

bool matches(const VersionReq& req, const SemVer& v) {
    if (req.op == VersionReq::Op::Exact) {
        return v == SemVer{req.major, req.minor, req.patch};
    }
    SemVer lo = lowerBound(req);
    SemVer hi = req.op == VersionReq::Op::Caret ? caretUpperBound(req) : tildeUpperBound(req);
    return v >= lo && v < hi;
}

std::optional<SemVer> pickBestSatisfying(const std::vector<SemVer>& available,
                                          const VersionReq& req) {
    std::optional<SemVer> best;
    for (const SemVer& v : available) {
        if (matches(req, v) && (!best || v > *best)) best = v;
    }
    return best;
}

} // namespace ebpm
