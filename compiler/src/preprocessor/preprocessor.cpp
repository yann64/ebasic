#include "preprocessor/preprocessor.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebasic {

namespace fs = std::filesystem;

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

// Parses `#include ["once"] "path"`. `rest` is everything after the
// "include" keyword. Returns false (with no diagnostic - the caller does
// that) if it isn't well-formed.
bool parseIncludeDirective(std::string rest, bool& once, std::string& path) {
    rest = trimLeft(rest);
    once = false;
    if (rest.rfind("once", 0) == 0 && rest.size() > 4 && !isIdentChar(rest[4])) {
        once = true;
        rest = trimLeft(rest.substr(4));
    }
    if (rest.empty() || rest[0] != '"') return false;
    size_t end = rest.find('"', 1);
    if (end == std::string::npos) return false;
    path = rest.substr(1, end - 1);
    return true;
}

std::vector<std::string> splitLines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// State shared across the whole recursive expansion (spans #include
// boundaries), as opposed to each file's own #ifdef nesting, which is not
// (see expandSource's doc comment).
struct PPState {
    DiagnosticEngine& diags;
    std::unordered_map<std::string, std::string> macros;
    // Canonical absolute paths already brought in, by a plain #include or an
    // #include once, at least once - consulted (only) by #include once to
    // decide whether to skip a repeat inclusion.
    std::unordered_set<std::string> everIncluded;
    // Canonical absolute paths currently being expanded (i.e. an ancestor
    // #include is still open), for circular-#include detection.
    std::vector<std::string> activeStack;
    // M5: extra search paths, consulted in order only after the normal
    // includer-relative lookup fails - see preprocess()'s doc comment.
    std::vector<std::string> includeDirs;
};

void expandSource(PPState& state, const std::string& source, int fileId, const fs::path& dir,
                   std::ostringstream& out, std::vector<SourceLoc>& lineMap);

// Handles one #include/#include once directive: resolves the path, guards
// against cycles and (for `once`) repeats, then recurses. Always emits
// exactly one line (blank) for the directive itself into the parent's
// stream - the included file's own lines are emitted by the recursive call.
void handleInclude(PPState& state, const std::string& rawArg, const fs::path& dir, SourceLoc loc,
                    std::ostringstream& out, std::vector<SourceLoc>& lineMap) {
    bool once = false;
    std::string pathArg;
    if (!parseIncludeDirective(rawArg, once, pathArg)) {
        state.diags.error(loc, "expected #include [once] \"path\"");
        out << "\n";
        lineMap.push_back(loc);
        return;
    }

    fs::path requested(pathArg);
    fs::path resolved = requested.is_absolute() ? requested : (dir / requested);

    std::error_code ec;
    fs::path canonical = fs::canonical(resolved, ec);
    // M5: only fall back to the -I search list for a relative path whose
    // includer-relative lookup just failed - an absolute path has nowhere
    // else to resolve against, and a successful includer-relative lookup
    // always wins outright (matches a C/C++ compiler's own quote-include
    // search order: includer's directory first, -I list second).
    if (ec && !requested.is_absolute()) {
        for (const std::string& searchDir : state.includeDirs) {
            fs::path candidate = fs::path(searchDir) / requested;
            fs::path candidateCanonical = fs::canonical(candidate, ec);
            if (!ec) {
                canonical = candidateCanonical;
                break;
            }
        }
    }
    if (ec) {
        state.diags.error(loc, "cannot open included file '" + pathArg + "'");
        out << "\n";
        lineMap.push_back(loc);
        return;
    }
    std::string canonicalStr = canonical.string();

    out << "\n"; // the #include line itself
    lineMap.push_back(loc);

    if (once && state.everIncluded.count(canonicalStr)) {
        return; // already brought in; #include once means skip the repeat
    }
    for (const std::string& active : state.activeStack) {
        if (active == canonicalStr) {
            state.diags.error(loc, "circular #include detected involving '" + pathArg + "'");
            return;
        }
    }

    std::ifstream in(canonical);
    if (!in) {
        state.diags.error(loc, "cannot open included file '" + pathArg + "'");
        return;
    }
    std::ostringstream buf;
    buf << in.rdbuf();

    int includedFileId = state.diags.registerFile(canonicalStr);
    state.everIncluded.insert(canonicalStr);
    state.activeStack.push_back(canonicalStr);
    expandSource(state, buf.str(), includedFileId, canonical.parent_path(), out, lineMap);
    state.activeStack.pop_back();
}

// Expands one file's content (already read into `source`) into `out`/
// `lineMap`, recursing into handleInclude for any #include directives.
// #ifdef/#ifndef nesting is local to this file - it must be balanced within
// the file itself (an unclosed one is an error reported against this file),
// unlike macros, which are shared globally across the whole program so a
// #define in one file is visible in files included after it.
void expandSource(PPState& state, const std::string& source, int fileId, const fs::path& dir,
                   std::ostringstream& out, std::vector<SourceLoc>& lineMap) {
    std::vector<std::string> lines = splitLines(source);

    std::vector<bool> ifStack;
    auto activeNow = [&]() {
        for (bool b : ifStack) {
            if (!b) return false;
        }
        return true;
    };

    for (size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
        const std::string& raw = lines[lineNo];
        std::string trimmed = trimLeft(raw);
        SourceLoc loc{static_cast<int>(lineNo) + 1, 1, fileId};

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::istringstream ds(trimmed.substr(1));
            std::string kw;
            ds >> kw;

            if (kw == "include") {
                std::string rest;
                std::getline(ds, rest);
                if (activeNow()) {
                    handleInclude(state, rest, dir, loc, out, lineMap);
                } else {
                    out << "\n";
                    lineMap.push_back(loc);
                }
                continue;
            }
            if (kw == "define") {
                if (activeNow()) {
                    std::string name;
                    ds >> name;
                    std::string value;
                    std::getline(ds, value);
                    state.macros[name] = trimLeft(value);
                }
            } else if (kw == "ifdef" || kw == "ifndef") {
                std::string name;
                ds >> name;
                bool defined = state.macros.count(name) != 0;
                ifStack.push_back(kw == "ifdef" ? defined : !defined);
            } else if (kw == "else") {
                if (ifStack.empty()) {
                    state.diags.error(loc, "#else without matching #ifdef/#ifndef");
                } else {
                    ifStack.back() = !ifStack.back();
                }
            } else if (kw == "endif") {
                if (ifStack.empty()) {
                    state.diags.error(loc, "#endif without matching #ifdef/#ifndef");
                } else {
                    ifStack.pop_back();
                }
            } else {
                state.diags.error(loc, "unknown preprocessor directive '#" + kw + "'");
            }
            out << "\n";
            lineMap.push_back(loc);
            continue;
        }

        if (!activeNow()) {
            out << "\n";
            lineMap.push_back(loc);
            continue;
        }

        out << expandMacros(raw, state.macros) << "\n";
        lineMap.push_back(loc);
    }

    if (!ifStack.empty()) {
        state.diags.error(SourceLoc{static_cast<int>(lines.size()) + 1, 1, fileId}, "missing #endif");
    }
}

} // namespace

PreprocessResult preprocess(const std::string& mainSource, const std::string& mainPath,
                            DiagnosticEngine& diags, const std::vector<std::string>& includeDirs) {
    PPState state{diags, {}, {}, {}, includeDirs};
    int fileId = diags.registerFile(mainPath);

    std::error_code ec;
    fs::path canonicalMain = fs::canonical(mainPath, ec);
    fs::path dir = ec ? fs::path(mainPath).parent_path() : canonicalMain.parent_path();
    if (!ec) {
        state.everIncluded.insert(canonicalMain.string());
        state.activeStack.push_back(canonicalMain.string());
    }

    PreprocessResult result;
    std::ostringstream out;
    expandSource(state, mainSource, fileId, dir, out, result.lineMap);
    result.source = out.str();
    return result;
}

} // namespace ebasic
