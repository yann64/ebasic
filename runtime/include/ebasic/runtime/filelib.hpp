#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "ebasic/runtime/bstring.hpp"

/// FreeBASIC-style file creation/modification/deletion procedures
/// (KILL/MKDIR/RMDIR/NAME-as-Rename/etc.) - eBasic's own standard
/// library, made available in every compiled program (never opt-in) via
/// the same compiler-injected `Extern "C++"` prelude the string library
/// already uses (see compiler/src/preprocessor/builtin_prelude.hpp).
/// This header is pulled in unconditionally by runtime.hpp - no linking
/// step needed, matching this runtime's own existing header-only design.
///
/// Scoped to plain filesystem operations (create/delete/rename/copy a
/// file, create/remove a directory, check existence/size, read/write a
/// whole file's contents) - not FreeBASIC's much larger OPEN/PRINT #/
/// INPUT #/GET/PUT streaming-file-handle API (a real statement-level
/// feature, genuinely bigger scope).
///
/// eBasic has no exception/error-handling construct at all (no ON ERROR/
/// TRY), so every function here returns a plain INTEGER status (nonzero =
/// success, matching this language's own TRUE = -1 convention and
/// `eb-gtk4`'s existing `WriteFileContents`/gboolean-style precedent)
/// rather than throwing - implemented via `std::filesystem`'s own
/// non-throwing `std::error_code` overloads throughout, so a real
/// filesystem error (permissions, a missing parent directory, ...) is
/// always a returned failure status, never a crash.
namespace ebasic::rt::filelib {

inline std::int32_t FileExists(BString path) {
    std::error_code ec;
    bool result = std::filesystem::exists(path.str(), ec);
    return (!ec && result) ? -1 : 0;
}

/// The file's size in bytes, or -1 if it doesn't exist/can't be stat'd -
/// distinguishes a real error from a genuinely empty file (a valid 0).
inline std::int64_t FileLen(BString path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path.str(), ec);
    if (ec) return -1;
    return static_cast<std::int64_t>(size);
}

inline std::int32_t Kill(BString path) {
    std::error_code ec;
    bool removed = std::filesystem::remove(path.str(), ec);
    return (!ec && removed) ? -1 : 0;
}

/// Creates one new directory level - fails (like real FreeBASIC's own
/// MkDir) if `path` already exists, and never creates missing parent
/// directories ("mkdir -p" is deliberately out of scope).
inline std::int32_t MkDir(BString path) {
    std::error_code ec;
    bool created = std::filesystem::create_directory(path.str(), ec);
    return (!ec && created) ? -1 : 0;
}

/// Removes an *empty* directory - fails on a non-empty one or a plain
/// file (checked explicitly first, so this never silently deletes a
/// file the way a bare `std::filesystem::remove` would).
inline std::int32_t RmDir(BString path) {
    std::error_code ec;
    bool isDir = std::filesystem::is_directory(path.str(), ec);
    if (ec || !isDir) return 0;

    bool removed = std::filesystem::remove(path.str(), ec);
    return (!ec && removed) ? -1 : 0;
}

/// FreeBASIC's `Name oldfile As newfile` statement, as an ordinary
/// function - renamed from "Name" (which would collide with the
/// extremely common `DIM name AS STRING` identifier pattern).
inline std::int32_t Rename(BString oldPath, BString newPath) {
    std::error_code ec;
    std::filesystem::rename(oldPath.str(), newPath.str(), ec);
    return ec ? 0 : -1;
}

inline std::int32_t FileCopy(BString source, BString destination) {
    std::error_code ec;
    bool copied = std::filesystem::copy_file(
        source.str(), destination.str(),
        std::filesystem::copy_options::overwrite_existing, ec);
    return (!ec && copied) ? -1 : 0;
}

/// Reads `path`'s entire contents - `ok` (an out-parameter, matching
/// `eb-gtk4`'s own `ReadFileContents` convention) is set to whether the
/// file was actually opened and read successfully; `""` either way if
/// not (a genuinely empty file and a failed read both return `""`, only
/// `ok` tells them apart).
inline BString ReadFile(BString path, std::int32_t& ok) {
    std::ifstream in(path.str(), std::ios::binary);
    if (!in) {
        ok = 0;
        return BString("");
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    if (!in.good() && !in.eof()) {
        ok = 0;
        return BString("");
    }
    ok = -1;
    return BString(buf.str());
}

/// Writes `contents` to `path` - overwrites by default; `append <> 0`
/// appends instead (the new default-parameter feature lets the prelude
/// declare this trailing arg with a default of 0, so most callers never
/// need to pass it at all).
inline std::int32_t WriteFile(BString path, BString contents, std::int32_t append) {
    std::ios::openmode mode = std::ios::binary | (append != 0 ? std::ios::app : std::ios::trunc);
    std::ofstream out(path.str(), mode);
    if (!out) return 0;
    out << contents.str();
    return out.good() ? -1 : 0;
}

} // namespace ebasic::rt::filelib

// Thin global forwarders - the compiler's own builtin prelude declares
// these exact bare names via `Extern "C++"` with no `Namespace` block (so
// they're callable unqualified from any .bas program), which means the
// real, linkable C++ symbol must be a plain top-level function of that
// same name - the actual logic stays namespaced above.
inline std::int32_t FileExists(::ebasic::rt::BString path) {
    return ::ebasic::rt::filelib::FileExists(std::move(path));
}
inline std::int64_t FileLen(::ebasic::rt::BString path) {
    return ::ebasic::rt::filelib::FileLen(std::move(path));
}
inline std::int32_t Kill(::ebasic::rt::BString path) {
    return ::ebasic::rt::filelib::Kill(std::move(path));
}
inline std::int32_t MkDir(::ebasic::rt::BString path) {
    return ::ebasic::rt::filelib::MkDir(std::move(path));
}
inline std::int32_t RmDir(::ebasic::rt::BString path) {
    return ::ebasic::rt::filelib::RmDir(std::move(path));
}
inline std::int32_t Rename(::ebasic::rt::BString oldPath, ::ebasic::rt::BString newPath) {
    return ::ebasic::rt::filelib::Rename(std::move(oldPath), std::move(newPath));
}
inline std::int32_t FileCopy(::ebasic::rt::BString source, ::ebasic::rt::BString destination) {
    return ::ebasic::rt::filelib::FileCopy(std::move(source), std::move(destination));
}
inline ::ebasic::rt::BString ReadFile(::ebasic::rt::BString path, std::int32_t& ok) {
    return ::ebasic::rt::filelib::ReadFile(std::move(path), ok);
}
inline std::int32_t WriteFile(::ebasic::rt::BString path, ::ebasic::rt::BString contents, std::int32_t append) {
    return ::ebasic::rt::filelib::WriteFile(std::move(path), std::move(contents), append);
}
