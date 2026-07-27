#include "preprocessor/preprocessor.hpp"

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace ebasic {

namespace {

std::string trimLeft(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Expands macro references in `line`, skipping over string-literal contents
// (so a macro name that happens to appear inside a "..." string is left
// alone) and stopping at a ' line comment (its contents are never expanded).
std::string expandMacros(const std::string& line, const std::unordered_map<std::string, std::string>& macros) {
    std::string out;
    size_t i = 0;
    bool inString = false;
    while (i < line.size()) {
        char c = line[i];
        if (inString) {
            out.push_back(c);
            if (c == '"') inString = false;
            ++i;
            continue;
        }
        if (c == '"') {
            inString = true;
            out.push_back(c);
            ++i;
            continue;
        }
        if (c == '\'') {
            out += line.substr(i);
            break;
        }
        if (isIdentChar(c) && (i == 0 || !isIdentChar(line[i - 1]))) {
            size_t start = i;
            while (i < line.size() && isIdentChar(line[i])) ++i;
            std::string word = line.substr(start, i - start);
            auto it = macros.find(word);
            out += (it != macros.end()) ? it->second : word;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

} // namespace

std::string preprocess(const std::string& source, DiagnosticEngine& diags) {
    std::vector<std::string> lines;
    {
        std::istringstream in(source);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }

    std::unordered_map<std::string, std::string> macros;
    // Raw #ifdef/#ifndef condition per open level. Whether code is actually
    // emitted is `activeNow()`, the AND of every level - so a false ancestor
    // always wins regardless of what #else does to a nested level's own bit.
    std::vector<bool> ifStack;
    auto activeNow = [&]() {
        for (bool b : ifStack) {
            if (!b) return false;
        }
        return true;
    };

    std::ostringstream out;
    for (size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
        const std::string& raw = lines[lineNo];
        std::string trimmed = trimLeft(raw);
        SourceLoc loc{static_cast<int>(lineNo) + 1, 1};

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::istringstream ds(trimmed.substr(1));
            std::string kw;
            ds >> kw;

            if (kw == "define") {
                if (activeNow()) {
                    std::string name;
                    ds >> name;
                    std::string value;
                    std::getline(ds, value);
                    macros[name] = trimLeft(value);
                }
            } else if (kw == "ifdef" || kw == "ifndef") {
                std::string name;
                ds >> name;
                bool defined = macros.count(name) != 0;
                ifStack.push_back(kw == "ifdef" ? defined : !defined);
            } else if (kw == "else") {
                if (ifStack.empty()) {
                    diags.error(loc, "#else without matching #ifdef/#ifndef");
                } else {
                    ifStack.back() = !ifStack.back();
                }
            } else if (kw == "endif") {
                if (ifStack.empty()) {
                    diags.error(loc, "#endif without matching #ifdef/#ifndef");
                } else {
                    ifStack.pop_back();
                }
            } else {
                diags.error(loc, "unknown preprocessor directive '#" + kw + "'");
            }
            out << "\n";
            continue;
        }

        if (!activeNow()) {
            out << "\n";
            continue;
        }

        out << expandMacros(raw, macros) << "\n";
    }

    if (!ifStack.empty()) {
        diags.error(SourceLoc{static_cast<int>(lines.size()) + 1, 1}, "missing #endif");
    }

    return out.str();
}

} // namespace ebasic
