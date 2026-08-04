#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "ebasic/runtime/bstring.hpp"

/// FreeBASIC-style process/environment/program-control procedures -
/// eBasic's own standard library, made available in every compiled
/// program (never opt-in) via the same compiler-injected `Extern "C++"`
/// prelude the string/file/math/date-time libraries already use (see
/// compiler/src/preprocessor/builtin_prelude.hpp). Pulled in
/// unconditionally by runtime.hpp - no linking step needed.
///
/// `Command()` needs the program's own argc/argv, which a generated
/// program's `main()` didn't receive before this library - Codegen now
/// emits `int main(int argc, char** argv)` and calls
/// `setCommandLineArgs` with them before running any user code (see
/// Codegen::generate's own main() emission).
namespace ebasic::rt::processlib {

namespace detail {

inline std::vector<std::string>& commandLineArgs() {
    static std::vector<std::string> args;
    return args;
}

} // namespace detail

/// Called once, at the very top of generated main() - stores every
/// argument after argv[0] (the program's own path, which FreeBASIC's
/// own Command$ excludes too).
inline void setCommandLineArgs(int argc, char** argv) {
    std::vector<std::string>& args = detail::commandLineArgs();
    args.clear();
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
}

inline BString Environ(BString name) {
    const char* value = std::getenv(name.str().c_str());
    return BString(value ? std::string(value) : std::string());
}

/// The whole command line as one space-joined string, not including the
/// program's own path - matching real FreeBASIC's own Command$.
inline BString Command() {
    const std::vector<std::string>& args = detail::commandLineArgs();
    std::string joined;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) joined.push_back(' ');
        joined += args[i];
    }
    return BString(joined);
}

/// Runs `command` via the system shell, returning its exit status (or
/// -1 if the shell itself couldn't be started).
inline std::int32_t Shell(BString command) {
    int status = std::system(command.str().c_str());
    if (status == -1) return -1;
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

/// Blocks the calling program for `milliseconds` - unlike real
/// FreeBASIC's own Sleep, there's no no-argument/negative "wait for a
/// keypress" form (console input is out of scope for this library); a
/// negative value sleeps for zero.
inline void Sleep(std::int32_t milliseconds) {
    if (milliseconds <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

/// Terminates the program immediately with the given exit code -
/// distinct from the `END` keyword, which always closes a block
/// (`IF`/`SUB`/... ) and never means "stop the program" on its own.
inline void ExitProcess(std::int32_t code) { std::exit(code); }

} // namespace ebasic::rt::processlib

// Thin global forwarders - the compiler's own builtin prelude declares
// these exact bare names via `Extern "C++"` with no `Namespace` block
// (so they're callable unqualified from any .bas program), which means
// the real, linkable C++ symbol must be a plain top-level function of
// that same name - the actual logic stays namespaced above.
inline ::ebasic::rt::BString Environ(::ebasic::rt::BString name) {
    return ::ebasic::rt::processlib::Environ(std::move(name));
}
inline ::ebasic::rt::BString Command() { return ::ebasic::rt::processlib::Command(); }
inline std::int32_t Shell(::ebasic::rt::BString command) {
    return ::ebasic::rt::processlib::Shell(std::move(command));
}
inline void Sleep(std::int32_t milliseconds) { ::ebasic::rt::processlib::Sleep(milliseconds); }
inline void ExitProcess(std::int32_t code) { ::ebasic::rt::processlib::ExitProcess(code); }
