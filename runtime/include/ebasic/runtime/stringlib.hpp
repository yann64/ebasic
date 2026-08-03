#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>

#include "ebasic/runtime/bstring.hpp"

/// FreeBASIC-style string manipulation procedures (LEN/MID/LEFT/RIGHT/
/// INSTR/etc.) - eBasic's own standard library, made available in every
/// compiled program (never opt-in) via a compiler-injected `Extern "C++"`
/// prelude (see compiler/src/preprocessor/builtin_prelude.hpp) that
/// declares each function below by its exact bare name. This header is
/// pulled in unconditionally by runtime.hpp, so no linking step is needed
/// for any of it - matches this runtime's own existing header-only design
/// (see bstring.hpp/print.hpp).
///
/// Every parameter is taken by value (a plain BString/std::int32_t/double,
/// never a reference) - these are all pure, read-only operations on their
/// input, so a copy avoids any accidental-mutation-through-reference
/// question entirely, matching this project's own eb-cjson `JsonValue`-
/// BYVAL precedent.
///
/// ASCII/byte-oriented throughout (no Unicode awareness) - matches this
/// project's existing posture everywhere else (no WSTRING, no encoding-
/// aware anything yet). Every count/position argument is clamped into a
/// valid range rather than raising an error on out-of-range input, matching
/// FreeBASIC's own forgiving behavior (documented per-function below).
namespace ebasic::rt::strlib {

/// The number of characters in `s`.
inline std::int32_t Len(BString s) {
    return static_cast<std::int32_t>(s.str().size());
}

/// The leftmost `count` characters of `s` - `count` is clamped into
/// `[0, Len(s)]` (a negative or overly large count never errors).
inline BString Left(BString s, std::int32_t count) {
    const std::string& str = s.str();
    auto len = static_cast<std::int32_t>(str.size());
    std::int32_t n = count < 0 ? 0 : (count > len ? len : count);
    return BString(str.substr(0, static_cast<std::size_t>(n)));
}

/// The rightmost `count` characters of `s` - same clamping as `Left`.
inline BString Right(BString s, std::int32_t count) {
    const std::string& str = s.str();
    auto len = static_cast<std::int32_t>(str.size());
    std::int32_t n = count < 0 ? 0 : (count > len ? len : count);
    return BString(str.substr(static_cast<std::size_t>(len - n)));
}

/// `length` characters of `s` starting at the 1-based position `start`
/// (FreeBASIC's own indexing convention) - `start` is clamped into
/// `[1, Len(s) + 1]` (past-the-end gives ""), and `length` into
/// `[0, <characters actually available from start>]`. Passing a `length`
/// of at least `Len(s)` (the default) means "everything from `start` to
/// the end", reached here via a default parameter value rather than
/// FreeBASIC's real 2-arg/3-arg `MID$` overloading (which eBasic doesn't
/// have).
inline BString Mid(BString s, std::int32_t start, std::int32_t length) {
    const std::string& str = s.str();
    auto len = static_cast<std::int32_t>(str.size());
    std::int32_t startIdx = start < 1 ? 1 : (start > len + 1 ? len + 1 : start);
    std::int32_t available = len - (startIdx - 1);
    std::int32_t n = length < 0 ? 0 : (length > available ? available : length);
    return BString(str.substr(static_cast<std::size_t>(startIdx - 1), static_cast<std::size_t>(n)));
}

/// The 1-based position of the first occurrence of `needle` in `haystack`
/// at or after the 1-based position `start` (default 1, i.e. search the
/// whole string) - 0 if not found. FreeBASIC's own optional leading
/// `start` parameter is moved to trail here (a default value must trail
/// in eBasic - see docs/reference/procedures-and-arrays.md).
inline std::int32_t InStr(BString haystackArg, BString needleArg, std::int32_t start) {
    const std::string& haystack = haystackArg.str();
    const std::string& needle = needleArg.str();
    auto len = static_cast<std::int32_t>(haystack.size());
    std::int32_t startIdx = start < 1 ? 1 : (start > len + 1 ? len + 1 : start);
    std::size_t pos = haystack.find(needle, static_cast<std::size_t>(startIdx - 1));
    if (pos == std::string::npos) return 0;
    return static_cast<std::int32_t>(pos) + 1;
}

/// Like `InStr`, but searches backward from `start` (a 1-based position;
/// the default -1 means "search backward from the end") - 0 if not found.
inline std::int32_t InStrRev(BString haystackArg, BString needleArg, std::int32_t start) {
    const std::string& haystack = haystackArg.str();
    const std::string& needle = needleArg.str();
    auto len = static_cast<std::int32_t>(haystack.size());
    std::size_t searchFrom;
    if (start < 0) {
        searchFrom = std::string::npos;
    } else {
        std::int32_t startIdx = start > len ? len : start;
        searchFrom = startIdx <= 1 ? 0 : static_cast<std::size_t>(startIdx - 1);
    }
    std::size_t pos = haystack.rfind(needle, searchFrom);
    if (pos == std::string::npos) return 0;
    return static_cast<std::int32_t>(pos) + 1;
}

/// `s` with every letter uppercased (ASCII only).
inline BString UCase(BString s) {
    std::string result = s.str();
    for (char& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return BString(std::move(result));
}

/// `s` with every letter lowercased (ASCII only).
inline BString LCase(BString s) {
    std::string result = s.str();
    for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return BString(std::move(result));
}

/// `s` with every leading character found in `chars` (default: a plain
/// space) removed.
inline BString LTrim(BString s, BString charsArg) {
    const std::string& str = s.str();
    const std::string& chars = charsArg.str();
    std::size_t pos = str.find_first_not_of(chars);
    if (pos == std::string::npos) return BString("");
    return BString(str.substr(pos));
}

/// `s` with every trailing character found in `chars` removed.
inline BString RTrim(BString s, BString charsArg) {
    const std::string& str = s.str();
    const std::string& chars = charsArg.str();
    std::size_t pos = str.find_last_not_of(chars);
    if (pos == std::string::npos) return BString("");
    return BString(str.substr(0, pos + 1));
}

/// Both `LTrim` and `RTrim` together.
inline BString Trim(BString s, BString charsArg) {
    /// Two statements, not `RTrim(LTrim(s, charsArg), std::move(charsArg))` -
    /// C++ leaves the evaluation order of a call's own arguments
    /// unspecified, so `std::move(charsArg)` could run *before* `LTrim`'s
    /// call, leaving `LTrim` reading an already-moved-from `charsArg` (a
    /// real bug, caught by a direct throwaway comparison against real
    /// output before this ever reached the compiler-injected prelude).
    BString left = LTrim(std::move(s), charsArg);
    return RTrim(std::move(left), std::move(charsArg));
}

/// `n` formatted as text - a whole-number value never gets a trailing
/// ".0" (unlike a bare `std::to_string(double)`); eBasic has no function
/// overloading, so a single `DOUBLE`-typed `Str` covers `INTEGER` too via
/// this language's own implicit `INTEGER` -> `DOUBLE` argument widening.
inline BString Str(double n) {
    if (n == static_cast<double>(static_cast<long long>(n)) &&
        n > -1e15 && n < 1e15) {
        return BString(std::to_string(static_cast<long long>(n)));
    }
    std::ostringstream oss;
    oss.precision(15);
    oss << n;
    return BString(oss.str());
}

/// Parses `s`'s leading numeric text (skipping leading whitespace,
/// matching FreeBASIC's own lenient behavior) - 0.0 if nothing parses.
inline double Val(BString s) {
    const std::string& str = s.str();
    const char* cstr = str.c_str();
    char* endPtr = nullptr;
    double result = std::strtod(cstr, &endPtr);
    if (endPtr == cstr) return 0.0;
    return result;
}

/// A one-character string holding the given 8-bit character code
/// (`code` is masked into `[0, 255]`).
inline BString Chr(std::int32_t code) {
    auto byte = static_cast<unsigned char>(code & 0xFF);
    return BString(std::string(1, static_cast<char>(byte)));
}

/// The character code of `s`'s first character - 0 for an empty string.
inline std::int32_t Asc(BString s) {
    const std::string& str = s.str();
    if (str.empty()) return 0;
    return static_cast<std::int32_t>(static_cast<unsigned char>(str[0]));
}

/// A string of `n` spaces (`n` clamped to `>= 0`).
inline BString Space(std::int32_t n) {
    std::int32_t count = n < 0 ? 0 : n;
    return BString(std::string(static_cast<std::size_t>(count), ' '));
}

/// `s` repeated `n` times (`n` clamped to `>= 0`) - a small generalization
/// of FreeBASIC's `STRING$(n, char)` (a single repeated character); named
/// `Repeat` rather than `String` since `STRING` is already eBasic's
/// reserved type keyword.
inline BString Repeat(std::int32_t n, BString sArg) {
    std::int32_t count = n < 0 ? 0 : n;
    const std::string& s = sArg.str();
    std::string result;
    result.reserve(s.size() * static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) result += s;
    return BString(std::move(result));
}

} // namespace ebasic::rt::strlib

// Thin global forwarders - the compiler's own builtin prelude declares
// these exact bare names via `Extern "C++"` with no `Namespace` block (so
// they're callable unqualified from any .bas program), which means the
// real, linkable C++ symbol must be a plain top-level function of that
// same name - the actual logic stays namespaced above.
inline std::int32_t Len(::ebasic::rt::BString s) { return ::ebasic::rt::strlib::Len(std::move(s)); }
inline ::ebasic::rt::BString Left(::ebasic::rt::BString s, std::int32_t count) {
    return ::ebasic::rt::strlib::Left(std::move(s), count);
}
inline ::ebasic::rt::BString Right(::ebasic::rt::BString s, std::int32_t count) {
    return ::ebasic::rt::strlib::Right(std::move(s), count);
}
inline ::ebasic::rt::BString Mid(::ebasic::rt::BString s, std::int32_t start, std::int32_t length) {
    return ::ebasic::rt::strlib::Mid(std::move(s), start, length);
}
inline std::int32_t InStr(::ebasic::rt::BString haystack, ::ebasic::rt::BString needle, std::int32_t start) {
    return ::ebasic::rt::strlib::InStr(std::move(haystack), std::move(needle), start);
}
inline std::int32_t InStrRev(::ebasic::rt::BString haystack, ::ebasic::rt::BString needle, std::int32_t start) {
    return ::ebasic::rt::strlib::InStrRev(std::move(haystack), std::move(needle), start);
}
inline ::ebasic::rt::BString UCase(::ebasic::rt::BString s) { return ::ebasic::rt::strlib::UCase(std::move(s)); }
inline ::ebasic::rt::BString LCase(::ebasic::rt::BString s) { return ::ebasic::rt::strlib::LCase(std::move(s)); }
inline ::ebasic::rt::BString LTrim(::ebasic::rt::BString s, ::ebasic::rt::BString chars) {
    return ::ebasic::rt::strlib::LTrim(std::move(s), std::move(chars));
}
inline ::ebasic::rt::BString RTrim(::ebasic::rt::BString s, ::ebasic::rt::BString chars) {
    return ::ebasic::rt::strlib::RTrim(std::move(s), std::move(chars));
}
inline ::ebasic::rt::BString Trim(::ebasic::rt::BString s, ::ebasic::rt::BString chars) {
    return ::ebasic::rt::strlib::Trim(std::move(s), std::move(chars));
}
inline ::ebasic::rt::BString Str(double n) { return ::ebasic::rt::strlib::Str(n); }
inline double Val(::ebasic::rt::BString s) { return ::ebasic::rt::strlib::Val(std::move(s)); }
inline ::ebasic::rt::BString Chr(std::int32_t code) { return ::ebasic::rt::strlib::Chr(code); }
inline std::int32_t Asc(::ebasic::rt::BString s) { return ::ebasic::rt::strlib::Asc(std::move(s)); }
inline ::ebasic::rt::BString Space(std::int32_t n) { return ::ebasic::rt::strlib::Space(n); }
inline ::ebasic::rt::BString Repeat(std::int32_t n, ::ebasic::rt::BString s) {
    return ::ebasic::rt::strlib::Repeat(n, std::move(s));
}
