#include "preprocessor/preprocessor.hpp"

#include "preprocessor/builtin_prelude.hpp"
#include "preprocessor/pp_expr.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
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

std::string trimRight(const std::string& s) {
    size_t i = s.size();
    while (i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t')) --i;
    return s.substr(0, i);
}

std::string trim(const std::string& s) { return trimRight(trimLeft(s)); }

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

/// One #define/#macro definition. An object-like #define (the common case)
/// has `isFunctionLike == false` and an empty `params`. `isMacroBlock`
/// distinguishes a multi-line #macro/#endmacro (whose body may itself
/// contain directives, and which may only be invoked as the entire content
/// of a source line - see tryExpandBlockMacroInvocation) from a
/// function-like #define (single-line body, freely usable inline within a
/// larger expression).
struct MacroDef {
    bool isFunctionLike = false;
    bool isMacroBlock = false;
    std::vector<std::string> params;
    /// True if the last entry in `params` collects every trailing argument
    /// beyond the fixed ones (FreeBASIC's `name...` variadic parameter).
    bool variadic = false;
    std::string body;

    static MacroDef object(std::string body) {
        MacroDef def;
        def.body = std::move(body);
        return def;
    }
};

/// Finds the ')' matching the '(' at `s[openIdx]`, skipping over nested
/// parens and the contents of "..." string literals. Returns npos if
/// unbalanced.
size_t findMatchingParen(const std::string& s, size_t openIdx) {
    int depth = 0;
    bool inString = false;
    for (size_t k = openIdx; k < s.size(); ++k) {
        char c = s[k];
        if (inString) {
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
            continue;
        }
        if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
            if (depth == 0) return k;
        }
    }
    return std::string::npos;
}

/// Splits the text between a macro call's parens by top-level commas
/// (respecting nested parens/quotes), trimming each piece. Preserves empty
/// entries between consecutive commas (FreeBASIC's own #macro examples
/// rely on this - see macro4.bas's `test2(5,6, 7, , 9, 10, ,,13, 14)`).
/// An all-blank/empty `s` yields zero arguments, not one empty argument.
std::vector<std::string> splitTopLevelArgs(const std::string& s) {
    std::vector<std::string> result;
    if (trim(s).empty()) return result;
    int depth = 0;
    bool inString = false;
    size_t start = 0;
    for (size_t k = 0; k < s.size(); ++k) {
        char c = s[k];
        if (inString) {
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
        } else if (c == ',' && depth == 0) {
            result.push_back(trim(s.substr(start, k - start)));
            start = k + 1;
        }
    }
    result.push_back(trim(s.substr(start)));
    return result;
}

/// Parses a `#define`/`#macro` header - `NAME` alone (object-like), or
/// `NAME(params...)` (function-like/#macro) - from `rest` (everything
/// after the directive keyword). `#define`'s function-like form requires
/// the '(' immediately after the name (no space) to be recognized as such,
/// matching FreeBASIC's own documented rule for telling it apart from an
/// object-like macro whose body happens to start with '('; `#macro`
/// always allows a space there (`spaceAllowedBeforeParen`), since it's
/// unambiguous (`#macro` has no object-like form at all).
bool parseDefineHeader(const std::string& rest, bool spaceAllowedBeforeParen, std::string& name, MacroDef& def) {
    std::string s = trimLeft(rest);
    size_t i = 0;
    if (i >= s.size() || !(std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_')) return false;
    size_t start = i;
    while (i < s.size() && isIdentChar(s[i])) ++i;
    name = s.substr(start, i - start);

    size_t parenIdx = i;
    if (spaceAllowedBeforeParen) {
        while (parenIdx < s.size() && (s[parenIdx] == ' ' || s[parenIdx] == '\t')) ++parenIdx;
    }
    if (parenIdx < s.size() && s[parenIdx] == '(') {
        size_t close = findMatchingParen(s, parenIdx);
        if (close == std::string::npos) return false;
        def.isFunctionLike = true;
        std::string paramsText = s.substr(parenIdx + 1, close - parenIdx - 1);
        std::vector<std::string> rawParams = splitTopLevelArgs(paramsText);
        def.variadic = false;
        def.params.clear();
        for (size_t p = 0; p < rawParams.size(); ++p) {
            std::string pname = rawParams[p];
            bool isLast = (p + 1 == rawParams.size());
            if (isLast && pname.size() > 3 && pname.substr(pname.size() - 3) == "...") {
                pname = trim(pname.substr(0, pname.size() - 3));
                def.variadic = true;
            }
            def.params.push_back(pname);
        }
        def.body = trimLeft(s.substr(close + 1));
    } else {
        def.isFunctionLike = false;
        def.params.clear();
        def.variadic = false;
        def.body = trimLeft(s.substr(i));
    }
    return true;
}

/// Binds `rawArgs` (already split, already trimmed) to `def.params`,
/// filling `out`. Missing trailing arguments are bound to "" rather than
/// rejected (a deliberate leniency over strict FreeBASIC - see
/// docs/reference/preprocessor.md); too many arguments to a non-variadic
/// macro is reported via `diags` and returns false. A variadic macro's
/// last parameter collects every argument beyond the fixed ones, rejoined
/// with ", " - letting a macro body re-split it (as FreeBASIC's own
/// macro4.bas example does with Instr) exactly as if the original
/// comma-separated text had been passed as one argument.
bool bindMacroArgs(const MacroDef& def, const std::vector<std::string>& rawArgs, const std::string& macroName,
                    SourceLoc loc, DiagnosticEngine& diags, std::unordered_map<std::string, std::string>& out) {
    size_t fixedCount = def.variadic ? (def.params.empty() ? 0 : def.params.size() - 1) : def.params.size();
    if (!def.variadic && rawArgs.size() > fixedCount) {
        diags.error(loc, "too many arguments to macro '" + macroName + "'");
        return false;
    }
    for (size_t p = 0; p < fixedCount; ++p) {
        out[def.params[p]] = (p < rawArgs.size()) ? rawArgs[p] : "";
    }
    if (def.variadic) {
        std::string joined;
        for (size_t p = fixedCount; p < rawArgs.size(); ++p) {
            if (!joined.empty()) joined += ", ";
            joined += rawArgs[p];
        }
        out[def.params.back()] = joined;
    }
    return true;
}

/// Applies one macro body line's `#`(stringize)/`##`(concat)/plain
/// parameter substitution, honoring string-literal and `'`-comment
/// boundaries exactly like expandText does. Stringize (`#param`) must be
/// resolved before plain substitution (otherwise the `#` would be left
/// stranded in front of the already-substituted argument text instead of
/// quoting it); concat (`##`) is resolved last, splicing together
/// whatever plain text/argument text now flanks it.
std::string substituteMacroBodyLine(const std::string& line, const std::unordered_map<std::string, std::string>& paramToArg) {
    // Pass A: stringize.
    std::string afterStringize;
    {
        size_t i = 0;
        while (i < line.size()) {
            char c = line[i];
            if (c == '\'') {
                afterStringize += line.substr(i);
                break;
            }
            if (c == '"') {
                afterStringize.push_back(c);
                ++i;
                while (i < line.size()) {
                    afterStringize.push_back(line[i]);
                    bool end = (line[i] == '"');
                    ++i;
                    if (end) break;
                }
                continue;
            }
            if (c == '#' && i + 1 < line.size() && line[i + 1] == '#') {
                // A "##" concat marker - leave both characters alone for
                // Pass C, rather than letting the loop below reprocess the
                // second '#' as a fresh (bogus) stringize candidate.
                afterStringize.push_back('#');
                afterStringize.push_back('#');
                i += 2;
                continue;
            }
            if (c == '#') {
                size_t j = i + 1;
                while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) ++j;
                size_t idStart = j;
                while (j < line.size() && isIdentChar(line[j])) ++j;
                std::string ident = line.substr(idStart, j - idStart);
                auto it = paramToArg.find(ident);
                if (!ident.empty() && it != paramToArg.end()) {
                    // eBasic string literals escape an embedded '"' by
                    // doubling it (classic BASIC convention - see
                    // Lexer::lexString), not with a backslash.
                    afterStringize.push_back('"');
                    for (char ac : it->second) {
                        if (ac == '"') afterStringize.push_back('"');
                        afterStringize.push_back(ac);
                    }
                    afterStringize.push_back('"');
                    i = j;
                    continue;
                }
            }
            afterStringize.push_back(c);
            ++i;
        }
    }

    // Pass B: plain whole-word parameter substitution.
    std::string afterParams;
    {
        size_t i = 0;
        while (i < afterStringize.size()) {
            char c = afterStringize[i];
            if (c == '\'') {
                afterParams += afterStringize.substr(i);
                break;
            }
            if (c == '"') {
                afterParams.push_back(c);
                ++i;
                while (i < afterStringize.size()) {
                    afterParams.push_back(afterStringize[i]);
                    bool end = (afterStringize[i] == '"');
                    ++i;
                    if (end) break;
                }
                continue;
            }
            if (isIdentChar(c) && (i == 0 || !isIdentChar(afterStringize[i - 1]))) {
                size_t start = i;
                while (i < afterStringize.size() && isIdentChar(afterStringize[i])) ++i;
                std::string word = afterStringize.substr(start, i - start);
                auto it = paramToArg.find(word);
                afterParams += (it != paramToArg.end()) ? it->second : word;
                continue;
            }
            afterParams.push_back(c);
            ++i;
        }
    }

    // Pass C: concat.
    std::string result;
    {
        size_t i = 0;
        while (i < afterParams.size()) {
            char c = afterParams[i];
            if (c == '\'') {
                result += afterParams.substr(i);
                break;
            }
            if (c == '"') {
                result.push_back(c);
                ++i;
                while (i < afterParams.size()) {
                    result.push_back(afterParams[i]);
                    bool end = (afterParams[i] == '"');
                    ++i;
                    if (end) break;
                }
                continue;
            }
            if (c == '#' && i + 1 < afterParams.size() && afterParams[i + 1] == '#') {
                i += 2;
                continue;
            }
            result.push_back(c);
            ++i;
        }
    }
    return result;
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

std::string substituteMacroBody(const std::string& body, const std::unordered_map<std::string, std::string>& paramToArg) {
    std::vector<std::string> lines = splitLines(body);
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += "\n";
        result += substituteMacroBodyLine(lines[i], paramToArg);
    }
    return result;
}

/// State shared across the whole recursive expansion (spans #include
/// boundaries and #macro-body-invocation boundaries), as opposed to each
/// file's own #if/#ifdef nesting, which is not (see expandSource's doc
/// comment).
struct PPState {
    DiagnosticEngine& diags;
    std::unordered_map<std::string, MacroDef> macros;
    /// Canonical absolute paths already brought in, by a plain #include or an
    /// #include once, at least once - consulted (only) by #include once to
    /// decide whether to skip a repeat inclusion.
    std::unordered_set<std::string> everIncluded;
    /// Canonical absolute paths currently being expanded (i.e. an ancestor
    /// #include is still open), for circular-#include detection.
    std::vector<std::string> activeStack;
    /// M5: extra search paths, consulted in order only after the normal
    /// includer-relative lookup fails - see preprocess()'s doc comment.
    std::vector<std::string> includeDirs;
};

void expandSource(PPState& state, const std::string& source, int fileId, const fs::path& dir,
                   std::ostringstream& out, std::vector<SourceLoc>& lineMap);

/// Expands macro references in `text` - recursively, so a macro whose body
/// itself names another macro keeps expanding until nothing more can
/// change (matching FreeBASIC's own documented "done recursively" rule) -
/// while skipping over string-literal contents, stopping at a `'` line
/// comment, and never re-expanding a macro name already in `activeNames`
/// (the classic "painting" guard against `#define X X`-style infinite
/// recursion). `loc` supplies `__LINE__`/`__FILE__`'s value; both are
/// resolved before consulting `state.macros` at all, so a user `#define`
/// of the same name (not itself supported/tested, but harmless) can never
/// shadow them. Function-like macros are only expanded here when actually
/// called (`name(args)`, whitespace before '(' allowed at the call site
/// even though `#define`'s own header forbids it in the *declaration* -
/// matching the ordinary C-preprocessor convention that only the
/// declaration's function-like-vs-object-like disambiguation is
/// space-sensitive) - a function-like name used without a following '('
/// is left as plain text, also matching C.
std::string expandText(const std::string& text, PPState& state, std::vector<std::string>& activeNames, SourceLoc loc) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (c == '"') {
            out.push_back(c);
            ++i;
            while (i < text.size()) {
                out.push_back(text[i]);
                bool end = (text[i] == '"');
                ++i;
                if (end) break;
            }
            continue;
        }
        if (c == '\'') {
            out += text.substr(i);
            break;
        }
        if (isIdentChar(c) && (i == 0 || !isIdentChar(text[i - 1]))) {
            size_t start = i;
            while (i < text.size() && isIdentChar(text[i])) ++i;
            std::string word = text.substr(start, i - start);

            if (word == "__LINE__") {
                out += std::to_string(loc.line);
                continue;
            }
            if (word == "__FILE__") {
                out += "\"" + state.diags.fileName(loc.fileId) + "\"";
                continue;
            }

            bool isActive = false;
            for (const std::string& n : activeNames) {
                if (n == word) {
                    isActive = true;
                    break;
                }
            }
            auto it = isActive ? state.macros.end() : state.macros.find(word);
            if (it != state.macros.end()) {
                const MacroDef& def = it->second;
                if (!def.isFunctionLike) {
                    activeNames.push_back(word);
                    out += expandText(def.body, state, activeNames, loc);
                    activeNames.pop_back();
                    continue;
                }
                size_t j = i;
                while (j < text.size() && (text[j] == ' ' || text[j] == '\t')) ++j;
                if (j < text.size() && text[j] == '(') {
                    size_t close = findMatchingParen(text, j);
                    if (close != std::string::npos) {
                        std::string argsText = text.substr(j + 1, close - j - 1);
                        std::vector<std::string> rawArgs = splitTopLevelArgs(argsText);
                        std::unordered_map<std::string, std::string> paramToArg;
                        if (bindMacroArgs(def, rawArgs, word, loc, state.diags, paramToArg)) {
                            std::string substituted = substituteMacroBody(def.body, paramToArg);
                            activeNames.push_back(word);
                            out += expandText(substituted, state, activeNames, loc);
                            activeNames.pop_back();
                        }
                        i = close + 1;
                        continue;
                    }
                }
                // No call syntax follows: leave the bare name as text.
            }
            out += word;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

/// Resolves every `defined ( symbol )` in `expr` to a literal "1"/"0"
/// *before* any ordinary macro substitution runs on the rest of the
/// expression - `defined`'s whole point is to test whether a name is a
/// macro without expanding it, so it must never see the macro-expanded
/// form of its own operand (the same reason C's own `defined` is special-
/// cased ahead of macro expansion).
std::string resolveDefinedCalls(const std::string& expr, const std::unordered_map<std::string, MacroDef>& macros) {
    std::string out;
    size_t i = 0;
    while (i < expr.size()) {
        char c = expr[i];
        if (c == '"') {
            out.push_back(c);
            ++i;
            while (i < expr.size()) {
                out.push_back(expr[i]);
                bool end = (expr[i] == '"');
                ++i;
                if (end) break;
            }
            continue;
        }
        if (isIdentChar(c) && (i == 0 || !isIdentChar(expr[i - 1]))) {
            size_t start = i;
            while (i < expr.size() && isIdentChar(expr[i])) ++i;
            std::string word = expr.substr(start, i - start);
            if (word == "defined") {
                size_t j = i;
                while (j < expr.size() && (expr[j] == ' ' || expr[j] == '\t')) ++j;
                if (j < expr.size() && expr[j] == '(') {
                    size_t close = findMatchingParen(expr, j);
                    if (close != std::string::npos) {
                        std::string symbol = trim(expr.substr(j + 1, close - j - 1));
                        out += macros.count(symbol) ? "1" : "0";
                        i = close + 1;
                        continue;
                    }
                }
            }
            out += word;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

/// Evaluates a `#if`/`#elseif`/`#assert` expression's raw text end to end:
/// `defined()` first, then ordinary macro substitution, then the
/// arithmetic/string evaluator in pp_expr.cpp. On a malformed expression
/// (already reported to `state.diags`), treats it as false rather than
/// aborting the whole file.
bool evalIfCondition(PPState& state, const std::string& rawExprText, SourceLoc loc) {
    std::string afterDefined = resolveDefinedCalls(rawExprText, state.macros);
    std::vector<std::string> activeNames;
    std::string expanded = expandText(afterDefined, state, activeNames, loc);
    auto result = evalPreprocessorExpr(expanded, [&](const std::string& msg) { state.diags.error(loc, msg); });
    return result.has_value() && *result != 0;
}

/// Parses `#include ["once"] "path"`. `rest` is everything after the
/// "include" keyword. Returns false (with no diagnostic - the caller does
/// that) if it isn't well-formed.
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

/// Handles one #include/#include once directive: resolves the path, guards
/// against cycles and (for `once`) repeats, then recurses. Always emits
/// exactly one line (blank) for the directive itself into the parent's
/// stream - the included file's own lines are emitted by the recursive call.
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
    /// M5: only fall back to the -I search list for a relative path whose
    /// includer-relative lookup just failed - an absolute path has nowhere
    /// else to resolve against, and a successful includer-relative lookup
    /// always wins outright (matches a C/C++ compiler's own quote-include
    /// search order: includer's directory first, -I list second).
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

/// Recognizes and expands a `#macro`-defined ("block") macro invocation,
/// which - unlike a function-like `#define` - may only appear as the
/// entire (trimmed) content of a source line, since its body can itself
/// contain directives (`#if`/`#else`/`#endif`, ...) that must be
/// interpreted for real, not just textually substituted. Returns false
/// (having emitted nothing) if `trimmed` doesn't have this shape at all,
/// so the caller falls back to ordinary `expandText`; returns true once
/// it's handled the line, whether that succeeded or hit an argument-count
/// error (already reported to `state.diags`).
bool tryExpandBlockMacroInvocation(PPState& state, const std::string& trimmed, int fileId, const fs::path& dir,
                                    SourceLoc loc, std::ostringstream& out, std::vector<SourceLoc>& lineMap) {
    size_t i = 0;
    if (i >= trimmed.size() || !(std::isalpha(static_cast<unsigned char>(trimmed[i])) || trimmed[i] == '_')) {
        return false;
    }
    size_t start = i;
    while (i < trimmed.size() && isIdentChar(trimmed[i])) ++i;
    std::string name = trimmed.substr(start, i - start);

    auto it = state.macros.find(name);
    if (it == state.macros.end() || !it->second.isMacroBlock) return false;

    size_t j = i;
    while (j < trimmed.size() && (trimmed[j] == ' ' || trimmed[j] == '\t')) ++j;
    if (j >= trimmed.size() || trimmed[j] != '(') return false;
    size_t close = findMatchingParen(trimmed, j);
    if (close == std::string::npos) return false;
    std::string trailing = trim(trimmed.substr(close + 1));
    if (!trailing.empty() && trailing[0] != '\'') return false;

    std::string argsText = trimmed.substr(j + 1, close - j - 1);
    std::vector<std::string> rawArgs = splitTopLevelArgs(argsText);
    std::unordered_map<std::string, std::string> paramToArg;
    if (!bindMacroArgs(it->second, rawArgs, name, loc, state.diags, paramToArg)) {
        out << "\n";
        lineMap.push_back(loc);
        return true;
    }

    std::string substitutedBody = substituteMacroBody(it->second.body, paramToArg);

    // Recurse through the same directive-aware expander used for
    // #include, so any #if/#print/nested-macro-call inside the body is
    // handled for real - but every resulting line is attributed to this
    // invocation's own `loc`, not the throwaway per-invocation line
    // numbers `expandSource` assigns internally (matching the project's
    // existing "diagnostics from macro expansion point at the call site"
    // simplification for #define, extended here to #macro).
    std::ostringstream nestedOut;
    std::vector<SourceLoc> nestedLineMap;
    expandSource(state, substitutedBody, fileId, dir, nestedOut, nestedLineMap);

    std::vector<std::string> outLines = splitLines(nestedOut.str());
    if (outLines.empty()) outLines.push_back("");
    for (const std::string& ol : outLines) {
        out << ol << "\n";
        lineMap.push_back(loc);
    }
    return true;
}

/// One level of #if/#ifdef/#ifndef nesting. `active` gates whether the
/// current branch's lines are kept; `anyTaken` remembers whether some
/// earlier branch in this same chain already fired, so a later
/// #elseif/#elseifdef/#elseifndef/#else in the chain knows to stay
/// inactive even if its own condition would otherwise be true (matching
/// ordinary if/elseif short-circuiting); `sawElse` catches a stray
/// #elseif/#else appearing after the chain's #else.
struct IfFrame {
    bool active;
    bool anyTaken;
    bool sawElse = false;
};

/// Expands one file's content (already read into `source`) into `out`/
/// `lineMap`, recursing into handleInclude for any #include directives and
/// into itself (via tryExpandBlockMacroInvocation) for a #macro body.
/// #if/#ifdef/#ifndef nesting is local to this call - it must be balanced
/// within the file/macro-body itself (an unclosed one is an error reported
/// against this file), unlike macros, which are shared globally across the
/// whole program so a #define in one file is visible in files included
/// after it.
void expandSource(PPState& state, const std::string& source, int fileId, const fs::path& dir,
                   std::ostringstream& out, std::vector<SourceLoc>& lineMap) {
    std::vector<std::string> lines = splitLines(source);

    std::vector<IfFrame> ifStack;
    auto activeNow = [&]() {
        for (const IfFrame& f : ifStack) {
            if (!f.active) return false;
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
            std::string rest;
            std::getline(ds, rest);

            if (kw == "include") {
                if (activeNow()) {
                    handleInclude(state, rest, dir, loc, out, lineMap);
                } else {
                    out << "\n";
                    lineMap.push_back(loc);
                }
                continue;
            }
            if (kw == "macro") {
                std::string macroName;
                MacroDef def;
                def.isMacroBlock = true;
                bool headerOk = parseDefineHeader(rest, /*spaceAllowedBeforeParen=*/true, macroName, def) &&
                                def.isFunctionLike;

                out << "\n"; // the #macro header line itself
                lineMap.push_back(loc);

                std::vector<std::string> bodyLines;
                size_t j = lineNo + 1;
                bool closed = false;
                for (; j < lines.size(); ++j) {
                    if (trimLeft(lines[j]) == "#endmacro") {
                        closed = true;
                        break;
                    }
                    bodyLines.push_back(lines[j]);
                }
                for (size_t k = lineNo + 1; k <= j && k < lines.size(); ++k) {
                    out << "\n";
                    lineMap.push_back(SourceLoc{static_cast<int>(k) + 1, 1, fileId});
                }
                if (!closed) {
                    state.diags.error(loc, "missing #endmacro");
                } else if (!headerOk) {
                    state.diags.error(loc, "expected #macro NAME(params)");
                } else if (activeNow()) {
                    std::string joined;
                    for (size_t k = 0; k < bodyLines.size(); ++k) {
                        if (k > 0) joined += "\n";
                        joined += bodyLines[k];
                    }
                    def.body = joined;
                    state.macros[macroName] = def;
                }
                lineNo = j;
                continue;
            }
            if (kw == "endmacro") {
                state.diags.error(loc, "#endmacro without matching #macro");
                out << "\n";
                lineMap.push_back(loc);
                continue;
            }
            if (kw == "define") {
                if (activeNow()) {
                    std::string name;
                    MacroDef def;
                    if (parseDefineHeader(rest, /*spaceAllowedBeforeParen=*/false, name, def)) {
                        state.macros[name] = def;
                    } else {
                        state.diags.error(loc, "expected #define NAME [value] or #define NAME(params) body");
                    }
                }
            } else if (kw == "undef") {
                if (activeNow()) {
                    std::string name;
                    std::istringstream rs(rest);
                    rs >> name;
                    state.macros.erase(name);
                }
            } else if (kw == "ifdef" || kw == "ifndef") {
                std::string name;
                std::istringstream rs(rest);
                rs >> name;
                bool defined = state.macros.count(name) != 0;
                bool cond = (kw == "ifdef") ? defined : !defined;
                ifStack.push_back(IfFrame{cond, cond});
            } else if (kw == "if") {
                bool cond = evalIfCondition(state, rest, loc);
                ifStack.push_back(IfFrame{cond, cond});
            } else if (kw == "elseifdef" || kw == "elseifndef") {
                std::string name;
                std::istringstream rs(rest);
                rs >> name;
                bool defined = state.macros.count(name) != 0;
                bool cond = (kw == "elseifdef") ? defined : !defined;
                if (ifStack.empty()) {
                    state.diags.error(loc, "#" + kw + " without matching #if/#ifdef/#ifndef");
                } else {
                    IfFrame& f = ifStack.back();
                    if (f.sawElse) {
                        state.diags.error(loc, "#" + kw + " after #else");
                    } else if (f.anyTaken) {
                        f.active = false;
                    } else {
                        f.active = cond;
                        f.anyTaken = cond;
                    }
                }
            } else if (kw == "elseif") {
                bool cond = evalIfCondition(state, rest, loc);
                if (ifStack.empty()) {
                    state.diags.error(loc, "#elseif without matching #if/#ifdef/#ifndef");
                } else {
                    IfFrame& f = ifStack.back();
                    if (f.sawElse) {
                        state.diags.error(loc, "#elseif after #else");
                    } else if (f.anyTaken) {
                        f.active = false;
                    } else {
                        f.active = cond;
                        f.anyTaken = cond;
                    }
                }
            } else if (kw == "else") {
                if (ifStack.empty()) {
                    state.diags.error(loc, "#else without matching #if/#ifdef/#ifndef");
                } else {
                    IfFrame& f = ifStack.back();
                    if (f.sawElse) {
                        state.diags.error(loc, "#else after #else");
                    }
                    f.sawElse = true;
                    f.active = !f.anyTaken;
                    f.anyTaken = true;
                }
            } else if (kw == "endif") {
                if (ifStack.empty()) {
                    state.diags.error(loc, "#endif without matching #if/#ifdef/#ifndef");
                } else {
                    ifStack.pop_back();
                }
            } else if (kw == "print") {
                if (activeNow()) {
                    std::vector<std::string> activeNames;
                    std::cout << expandText(trimLeft(rest), state, activeNames, loc) << "\n";
                }
            } else if (kw == "error") {
                if (activeNow()) {
                    std::vector<std::string> activeNames;
                    state.diags.error(loc, expandText(trimLeft(rest), state, activeNames, loc));
                }
            } else if (kw == "assert") {
                if (activeNow()) {
                    if (!evalIfCondition(state, rest, loc)) {
                        state.diags.error(loc, "assertion failed: " + trim(rest));
                    }
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

        if (tryExpandBlockMacroInvocation(state, trimmed, fileId, dir, loc, out, lineMap)) {
            continue;
        }

        std::vector<std::string> activeNames;
        out << expandText(raw, state, activeNames, loc) << "\n";
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
    /// Auto-defined, real-FreeBASIC-precedent platform macros (confirmed via
    /// FreeBASIC's own docs: __FB_WIN32__/__FB_LINUX__/__FB_DARWIN__ are
    /// real, existing macro names - adopted as-is rather than invented, to
    /// maximize source portability with real FreeBASIC code, the actual
    /// point of "same syntax as FreeBASIC"). Only the current platform's
    /// macro is ever defined, mirroring C/C++'s own _WIN32/__linux__/
    /// __APPLE__ convention. __FB_HAIKU__ has no real FreeBASIC precedent
    /// (Haiku isn't a FreeBASIC target) but follows the same naming style
    /// for consistency. Mirrors the exact #ifdef _WIN32/__APPLE__/
    /// __HAIKU__/else pattern already established in process.cpp/
    /// gitdep.cpp - no new platform-detection logic invented here.
#ifdef _WIN32
    state.macros["__FB_WIN32__"] = MacroDef::object("");
#elif defined(__APPLE__)
    state.macros["__FB_DARWIN__"] = MacroDef::object("");
#elif defined(__HAIKU__)
    state.macros["__FB_HAIKU__"] = MacroDef::object("");
#else
    state.macros["__FB_LINUX__"] = MacroDef::object("");
#endif

    /// __DATE__/__TIME__ (real FreeBASIC macro names/formats: "mm-dd-yyyy"
    /// and "hh:mm:ss") are seeded once, here, as ordinary object-like
    /// macros - unlike __LINE__/__FILE__, which vary per use site and so
    /// are handled dynamically inside expandText instead.
    {
        std::time_t t = std::time(nullptr);
        std::tm tmv = *std::localtime(&t);
        char dateBuf[32];
        std::snprintf(dateBuf, sizeof dateBuf, "%02d-%02d-%04d", tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_year + 1900);
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof timeBuf, "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        state.macros["__DATE__"] = MacroDef::object("\"" + std::string(dateBuf) + "\"");
        state.macros["__TIME__"] = MacroDef::object("\"" + std::string(timeBuf) + "\"");
    }

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

    /// eBasic's own standard string library (LEN/MID/LEFT/RIGHT/INSTR/etc. -
    /// see builtin_prelude.hpp) is spliced in first, via the exact same
    /// expansion machinery `#include` itself uses, under its own dedicated
    /// synthetic "file" - so it's always available, in every compiled
    /// program, with no `#include` needed. Its own declarations never
    /// reference `dir`/`includeDirs` (no `#include` of its own), so passing
    /// the main file's own `dir` here is inert either way.
    int preludeFileId = diags.registerFile(kBuiltinPreludeFileName);
    expandSource(state, kBuiltinPreludeSource, preludeFileId, dir, out, result.lineMap);

    expandSource(state, mainSource, fileId, dir, out, result.lineMap);
    result.source = out.str();
    return result;
}

} // namespace ebasic
