#include "codegen/codegen.hpp"
#include "diagnostics/diagnostics.hpp"
#include "driver/process.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "sema/sema.hpp"

#include "ebasic/version.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::string cxx;
    bool keepCpp = false;
    /// Extra library search paths (`-L <dir>`, repeatable), forwarded as-is
    /// to the backend compiler. A `Lib "name"` clause (M4) only names a
    /// library, not a path to it - the path (e.g. a test's build directory)
    /// has to come from outside the .bas source, exactly like g++'s own
    /// -L/-l split.
    std::vector<std::string> libDirs;
    /// M5c: extra libraries to link (`-l <name>`, repeatable), forwarded as
    /// `-l<name>` after the auto-derived ones from the module's own `Lib
    /// "name"` clauses. Needed for a *transitive* dependency: if this
    /// module `#include`s only its direct dependency's interface file, its
    /// own `Module::externLibs` has no idea a transitively-needed library
    /// even exists (that library's `Lib` clause lives in a different,
    /// never-`#include`d interface file) - ebpm passes it explicitly here
    /// instead, exactly like it already must pass `-L` for the same reason.
    std::vector<std::string> extraLibNames;
    /// M5: build a library (static archive + auto-generated interface file)
    /// instead of an executable - see printUsage for the exact output shape.
    bool libMode = false;
    /// Build a real, dynamically loadable shared library (.so/.dylib/.dll)
    /// instead of an executable - see printUsage for the exact output
    /// shape. Mutually exclusive with libMode (checked in main()).
    bool sharedLibMode = false;
    /// M5: extra #include search paths (`-I <dir>`, repeatable) - a fallback
    /// only, consulted after the includer-relative lookup fails (see
    /// preprocess()'s doc comment). Lets a package's source #include a
    /// dependency's auto-generated interface file without knowing its exact
    /// relative filesystem path.
    std::vector<std::string> includeDirs;
    /// -v/--version and -h/--help short-circuit everything else in main() -
    /// parseArgs still returns true with these set even if no input file
    /// was given, since printing the version/usage is a valid, complete
    /// invocation on its own.
    bool showVersion = false;
    bool showHelp = false; ///< see showVersion
    /// Echoes each backend-compiler invocation's exact argv to stderr
    /// before running it. Not tied to any one feature - generically useful
    /// for diagnosing what ebc actually told the backend compiler to do -
    /// but the immediate reason it exists is to make MSVC PCH usage
    /// (`/Yu`/`/Fp`) deterministically verifiable in a test (grep this
    /// output) rather than inferred from wall-clock timing.
    bool verbose = false;
};

bool parseArgs(int argc, char** argv, Options& opts, std::string& err) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-o") {
            if (i + 1 >= args.size()) {
                err = "-o requires an argument";
                return false;
            }
            opts.outputPath = args[++i];
        } else if (a == "-cxx") {
            if (i + 1 >= args.size()) {
                err = "-cxx requires an argument";
                return false;
            }
            opts.cxx = args[++i];
        } else if (a == "-L") {
            if (i + 1 >= args.size()) {
                err = "-L requires an argument";
                return false;
            }
            opts.libDirs.push_back(args[++i]);
        } else if (a == "-I") {
            if (i + 1 >= args.size()) {
                err = "-I requires an argument";
                return false;
            }
            opts.includeDirs.push_back(args[++i]);
        } else if (a == "-l") {
            if (i + 1 >= args.size()) {
                err = "-l requires an argument";
                return false;
            }
            opts.extraLibNames.push_back(args[++i]);
        } else if (a == "--keep-cpp") {
            opts.keepCpp = true;
        } else if (a == "--lib") {
            opts.libMode = true;
        } else if (a == "-dll" || a == "--shared-lib") {
            opts.sharedLibMode = true;
        } else if (a == "-v" || a == "--version") {
            opts.showVersion = true;
        } else if (a == "-h" || a == "--help") {
            opts.showHelp = true;
        } else if (a == "--verbose") {
            opts.verbose = true;
        } else if (!a.empty() && a[0] == '-') {
            err = "unknown option: " + a;
            return false;
        } else {
            if (!opts.inputPath.empty()) {
                err = "multiple input files are not supported yet";
                return false;
            }
            opts.inputPath = a;
        }
    }
    if (opts.inputPath.empty() && !opts.showVersion && !opts.showHelp) {
        err = "no input file";
        return false;
    }
    if (opts.libMode && opts.sharedLibMode) {
        err = "--lib and --shared-lib/-dll are mutually exclusive";
        return false;
    }
    return true;
}

void printUsage(std::ostream& os) {
    os << "usage: ebc <input.bas> [-o <output>] [-cxx <compiler>] [-L <dir>]... [-I <dir>]...\n";
    os << "           [-l <name>]... [--keep-cpp] [--lib | --shared-lib] [--verbose]\n";
    os << "       ebc [-v | --version] [-h | --help]\n";
    os << "  --lib: build a library instead of an executable - <output> is a bare\n";
    os << "  name; produces lib<output>.a (a static archive), <output>.iface.bas\n";
    os << "  (an auto-generated interface for dependent packages to #include), and\n";
    os << "  <output>.libs (this archive's own Lib \"name\" clauses, one per line -\n";
    os << "  for a build tool to forward to a downstream consumer's own link step).\n";
    os << "  --shared-lib (alias -dll): build a real, dynamically loadable shared\n";
    os << "  library instead of an executable - <output> is a bare name; produces\n";
    os << "  a real lib<output>.so/.dylib (Linux/Haiku/macOS) or <output>.dll +\n";
    os << "  lib<output>.dll.a (Windows/MinGW import library), plus the same\n";
    os << "  <output>.iface.bas/<output>.libs sidecar files --lib produces. Only a\n";
    os << "  SUB/FUNCTION written with a real body inside an `Extern \"C\" ... End\n";
    os << "  Extern` block becomes a real, stable, unmangled export usable by\n";
    os << "  another program (e.g. dlopen'd) - everything else keeps its ordinary\n";
    os << "  mangled name, same as --lib. Mutually exclusive with --lib.\n";
    os << "  -I <dir>: extra #include search path, consulted only after the\n";
    os << "  includer-relative lookup fails.\n";
    os << "  -l <name>: extra library to link, alongside any already named by the\n";
    os << "  module's own Lib \"name\" clauses.\n";
    os << "  -cxx: g++/clang++-style flags are used by default; a backend named\n";
    os << "  'cl'/'cl.exe'/'clang-cl' (matched on the basename) switches to\n";
    os << "  MSVC-style flags automatically.\n";
    os << "  --verbose: echo each backend-compiler invocation's exact argv to\n";
    os << "  stderr before running it.\n";
}

/// M8e: resolves ebc's own on-disk location from argv[0], so
/// runtimeIncludeArgs (below) can look for an *installed* runtime sitting
/// alongside this executable before falling back to the build-tree path
/// baked in at compile time - a real install (Haiku package or otherwise)
/// can't rely on the build tree it was built in still existing.
fs::path resolveOwnExecutablePath(const std::string& argv0) {
    fs::path p(argv0);
    std::error_code ec;
    if (p.has_parent_path()) {
        fs::path resolved = fs::absolute(p, ec);
        if (ec) return p;
        fs::path canonical = fs::canonical(resolved, ec);
        return ec ? resolved : canonical;
    }
    /// A bare name (e.g. just "ebc", found via a PATH lookup by whatever
    /// launched us) - search PATH ourselves the same way, since argv[0]
    /// alone doesn't tell us where we actually live.
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
#ifdef _WIN32
        const char sep = ';';
#else
        const char sep = ':';
#endif
        std::string pathStr = pathEnv;
        size_t start = 0;
        while (start <= pathStr.size()) {
            size_t end = pathStr.find(sep, start);
            std::string dir = pathStr.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!dir.empty()) {
                fs::path candidate = fs::path(dir) / p;
                if (fs::exists(candidate, ec)) {
                    fs::path canonical = fs::canonical(candidate, ec);
                    return ec ? candidate : canonical;
                }
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return p;
}

/// Flatpak sandbox only: a host-spawned backend compiler (see
/// ebasic::hostExecArgs in process.cpp, used below when invoking it) runs
/// in the HOST's own mount namespace, where the sandbox-only /app/... path
/// passed as -I doesn't exist at all - confirmed live: g++ was found and
/// invoked correctly via flatpak-spawn, but then failed with a plain "No
/// such file or directory" on the runtime header, since /app is invisible
/// outside the sandbox. --filesystem=host already shares $HOME identically
/// between the sandbox and the host (confirmed live too - a file at
/// ~/some/path is visible, and readable, on both sides) - so copying the
/// runtime include/PCH directories into a host-visible cache directory
/// once, keyed by version (so an app update naturally invalidates a stale
/// copy), then handing the host compiler *that* path instead, sidesteps
/// the mismatch entirely. A no-op everywhere else (docker/bare-metal/every
/// other packaging format never has /.flatpak-info at all).
fs::path stageRuntimeDirForFlatpak(const fs::path& dir, const std::string& kind) {
    const char* home = std::getenv("HOME");
    if (!home) return dir; // no host-visible location known - caller keeps the original path
    std::string versionKey = ebasic::kProjectVersion;
    if (ebasic::kGitHash[0] != '\0') {
        versionKey += "-";
        versionKey += ebasic::kGitHash;
    }
    fs::path cacheDir = fs::path(home) / ".cache" / "ebasic" / ("runtime-" + versionKey) / kind;
    std::error_code ec;
    if (!fs::exists(cacheDir, ec)) {
        fs::create_directories(cacheDir, ec);
        fs::copy(dir, cacheDir, fs::copy_options::recursive, ec);
    }
    return cacheDir;
}

/// The literal header text codegen's generate() puts as the first #include
/// of every generated .cpp - shared here so the MSVC /Yc (build side, see
/// runtime/CMakeLists.txt) and /Yu (consume side, below) arguments use the
/// byte-identical string rather than each independently reconstructing it.
constexpr const char* kRuntimeHeaderInclude = "ebasic/runtime/runtime.hpp";

/// M6/M8e/M9 (real MSVC PCH): the runtime's PCH shadow directory and header
/// include dir. For GCC, added as extra -I entries (PCH dir first) so a
/// plain `#include "ebasic/runtime/runtime.hpp"` automatically prefers a
/// precompiled .gch sitting there - no other change to the compile
/// invocation is needed (verified empirically: GCC's own automatic PCH
/// lookup, and its own graceful fallback when the .gch doesn't match, both
/// require no special flags at all). MSVC has no such automatic lookup or
/// graceful fallback - cl.exe needs an explicit `/Yu"..."` + `/Fp<path>`
/// pair on every consuming compile, and a stale/mismatched .pch is a hard
/// error, not a silent reparse - so `usePch` (default true) lets the caller
/// retry once without them on that specific failure (see
/// runCompilerStepWithPchFallback), and the .pch's existence is always
/// checked explicitly first (never trusted the way a non-empty
/// EBASIC_RUNTIME_PCH_DIR is trusted for GCC's own build-tree fallback
/// below) - passing /Yu/Fp for a .pch that doesn't actually exist would be
/// an MSVC hard-error, not a harmless no-op the way an absent .gch is for
/// GCC. Tries the installed layout relative to ebc's own executable path
/// first (EBASIC_RUNTIME_INSTALL_RELDIR, see compiler/CMakeLists.txt and
/// runtime/CMakeLists.txt's install() rules), falling back to the
/// build-tree paths baked in at compile time - which is what every
/// existing dev/test workflow (running ebc straight from the build tree)
/// still gets, unchanged.
std::vector<std::string> runtimeIncludeArgs(const std::string& argv0, bool msvc, bool usePch = true) {
    std::vector<std::string> args;
    std::string pchFileName = msvc ? "runtime.pch" : "runtime.hpp.gch";

    auto appendPchFlags = [&](const fs::path& pchDir) {
        if (msvc) {
            /// No quotes around the header name (unlike some MSVC-flag
            /// examples elsewhere) - MSVC only needs `/Yu"..."` quoting to
            /// disambiguate a space in the filename, and quoting it here
            /// anyway once caused a real bug: CMake's NMake-Makefiles
            /// generator didn't carry a hand-escaped `\"` through to the
            /// literal argv byte-for-byte the way this file's own
            /// quoteArgvArgument (process.cpp) does, so cl.exe ended up
            /// receiving literal backslash-quote characters instead of a
            /// real `"` - a C2857 "#include ... could not be found"
            /// mismatch against runtime_pch.cpp's own unquoted #include
            /// text. Simpler to just never need quoting at all, on both
            /// the /Yc (runtime/CMakeLists.txt) and /Yu (here) sides.
            args.push_back(std::string("/Yu") + kRuntimeHeaderInclude);
            args.push_back("/Fp" + (pchDir / "ebasic" / "runtime" / pchFileName).string());
        } else {
            args.push_back("-I");
            args.push_back(pchDir.string());
        }
    };

    fs::path exeDir = resolveOwnExecutablePath(argv0).parent_path();
    fs::path installedBase = exeDir / EBASIC_RUNTIME_INSTALL_RELDIR;
    fs::path installedInclude = installedBase / "include";
    std::error_code ec;
    if (fs::exists(installedInclude / "ebasic" / "runtime" / "runtime.hpp", ec)) {
        fs::path effectiveInclude = installedInclude;
        fs::path installedPch = installedBase / "pch";
        bool hasPch = usePch && fs::exists(installedPch / "ebasic" / "runtime" / pchFileName, ec);
        fs::path effectivePch = installedPch;

        if (fs::exists("/.flatpak-info", ec)) {
            effectiveInclude = stageRuntimeDirForFlatpak(installedInclude, "include");
            if (hasPch) effectivePch = stageRuntimeDirForFlatpak(installedPch, "pch");
        }

        if (hasPch) appendPchFlags(effectivePch);
        args.push_back("-I");
        args.push_back(effectiveInclude.string());
        return args;
    }

    std::string pchDir = EBASIC_RUNTIME_PCH_DIR;
    if (!pchDir.empty()) {
        /// GCC keeps its own pre-existing behavior here (trust the
        /// non-empty compile-time constant, no runtime file-existence
        /// check) - see this function's own doc comment for why MSVC can't
        /// do the same.
        bool hasPch = usePch && (msvc ? fs::exists(fs::path(pchDir) / "ebasic" / "runtime" / pchFileName, ec)
                                       : true);
        if (hasPch) appendPchFlags(pchDir);
    }
    args.push_back("-I");
    args.push_back(EBASIC_RUNTIME_INCLUDE_DIR);
    return args;
}

/// MSVC-only, and only meaningful when `runtimeIncludes` (from
/// runtimeIncludeArgs) actually used the PCH: the path to
/// runtime_pch.obj, the inert-but-mandatory companion object the /Yc PCH-
/// creation compile always produces alongside the .pch itself (see
/// runtime/CMakeLists.txt's MSVC block). Unlike GCC's .gch, cl.exe
/// requires this specific object be present in the *link* whenever any
/// input .obj was compiled with `/Yu` against the same .pch - a real
/// MSVC linker requirement (LNK2011 "precompiled object missing from the
/// link" otherwise), confirmed empirically against real cl.exe, not
/// something the initial design anticipated. Derived from the already-
/// computed `/Fp<path>` token rather than re-deriving the installed-vs-
/// build-tree lookup a second time - same directory, sibling filename.
/// Returns an empty path when `runtimeIncludes` has no `/Fp` token at all
/// (PCH wasn't used for this invocation).
fs::path msvcRuntimePchObjectPath(const std::vector<std::string>& runtimeIncludes) {
    for (const std::string& a : runtimeIncludes) {
        if (a.rfind("/Fp", 0) == 0) {
            return fs::path(a.substr(3)).parent_path() / "runtime_pch.obj";
        }
    }
    return {};
}

/// Independent of whether THIS invocation's own compile step actually
/// used `/Yu` (which may have fallen back to no-PCH on a mismatch, or -
/// for a consuming exe/`--shared-lib` build - simply have nothing to do
/// with whether a *linked* static archive needed it): a linked-in
/// archive built by an earlier, separate `ebc --lib` invocation may
/// contain an object compiled with `/Yu` against this same PCH, which
/// still needs the companion object present in *this* link too - MSVC's
/// linker just needs *some* copy of the matching PCH-creation object
/// anywhere in the final link, not specifically one produced by this
/// invocation. Always including it whenever a real PCH exists right now,
/// regardless of this invocation's own PCH usage, is what makes that
/// transitively safe - the object is inert (see msvcRuntimePchObjectPath's
/// own doc comment), so this costs nothing on the many links where
/// nothing actually needed it. Robust within one consistent build
/// environment (PCH availability doesn't change between the `--lib`
/// build and whatever later links against it, e.g. one `ebpm build` run)
/// - not across environments with differing PCH availability, which is
/// an inherent limitation of MSVC's PCH model for any prebuilt artifact,
/// not something specific to this mechanism.
fs::path msvcRuntimePchObjectIfAvailable(const std::string& argv0, bool msvc) {
    if (!msvc) return {};
    return msvcRuntimePchObjectPath(runtimeIncludeArgs(argv0, msvc, /*usePch=*/true));
}

/// M5 (--lib mode, and --shared-lib/-dll): a library's object file must
/// never define `main` itself (it would collide with the consuming
/// package's own `main` at final link time), so its module may only contain
/// declarations - no top-level executable statement (PRINT, assignment, IF,
/// a loop, ...). Checked structurally here, directly against the parsed
/// module, rather than threading a new mode flag through Sema.
bool hasOnlyLibDeclarations(const ebasic::Module& module, std::string& err) {
    for (const auto& stmtPtr : module.stmts) {
        switch (stmtPtr->kind) {
            case ebasic::StmtKind::Dim:
            case ebasic::StmtKind::Const:
            case ebasic::StmtKind::Enum:
            case ebasic::StmtKind::SubDecl:
            case ebasic::StmtKind::FunctionDecl:
            case ebasic::StmtKind::TypeDecl:
            case ebasic::StmtKind::UnionDecl:
            case ebasic::StmtKind::NamespaceDecl:
                continue;
            default:
                err = "line " + std::to_string(stmtPtr->loc.line) +
                      ": a --lib/--shared-lib build may only contain declarations (DIM/CONST/"
                      "ENUM/SUB/FUNCTION/TYPE/UNION/NAMESPACE) at the top level, not executable "
                      "code";
                return false;
        }
    }
    return true;
}

/// Mirrors pkg/src/manifest.cpp's own currentTargetOS() idiom - duplicated
/// locally rather than shared across binaries for something this small (see
/// this project's own established preference, e.g. ebpm's build.cpp doing
/// the same). Only used by the --shared-lib link step below, to pick the
/// right artifact prefix/extension/link flags.
std::string currentTargetOS() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__HAIKU__)
    return "haiku";
#else
    return "linux";
#endif
}

/// True when `cxx`'s own basename identifies an MSVC-CLI-compatible
/// compiler driver (`cl.exe` itself, or `clang-cl` - Clang's MSVC-
/// compatible front end, which accepts the same `/`-style flags) rather
/// than the GCC/Clang-style (`-std=`, `-c ... -o`, `-shared`, `-l`/`-L`)
/// driver every other backend (g++, clang++, real Clang) uses. Matched on
/// the stem only (case-insensitively), so `-cxx`/`CXX` may name a bare
/// command (found via PATH) or a full path, with or without ".exe" - this
/// is the toolchain-abstraction layer the M8 Windows port's own notes
/// (docs/architecture/roadmap.md) deferred MSVC support on.
bool isMsvcToolchain(const std::string& cxx) {
    std::string stem = fs::path(cxx).stem().string();
    for (char& c : stem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return stem == "cl" || stem == "clang-cl";
}

/// Builds the "compile one .gen.cpp to one object file" step shared by all
/// three build modes (--lib, --shared-lib, plain executable) - they differ
/// only in what happens *after* this step (archive vs. link-as-DLL vs.
/// link-as-exe). `runtimeIncludes` is `runtimeIncludeArgs()`'s own output
/// (alternating "-I"/path tokens) - translated to "/I" for MSVC, since
/// cl.exe's PCH model is flag-driven rather than GCC's automatic same-
/// directory .gch lookup, so an MSVC build harmlessly ignores the (GCC-
/// only) .gch sitting in that first -I/-I entry and falls through to the
/// real header in the second, exactly like a plain clang++ build already
/// does today - no separate MSVC PCH implementation needed for this to be
/// *correct*, just not yet as fast as the GCC path. `positionIndependent`
/// mirrors the existing GCC/Clang `-fPIC` (meaningless on Windows/MSVC -
/// PE relocation works differently - so never requested there).
std::vector<std::string> compileToObjectArgs(const std::string& cxx, bool msvc,
                                              const std::vector<std::string>& runtimeIncludes,
                                              const fs::path& cppPath, const fs::path& objPath,
                                              bool positionIndependent) {
    std::vector<std::string> args = {cxx};
    if (msvc) {
        args.push_back("/nologo");
        args.push_back("/std:c++17");
        /// C++/WinRT and most real-world Windows C++ needs the standard
        /// (synchronous) exception-unwind model turned on explicitly -
        /// unlike GCC/Clang, MSVC doesn't enable it by default.
        args.push_back("/EHsc");
        for (const std::string& a : runtimeIncludes) args.push_back(a == "-I" ? "/I" : a);
        args.push_back("/c");
        args.push_back(cppPath.string());
        args.push_back("/Fo:" + objPath.string());
    } else {
        args.push_back("-std=c++17");
        for (const std::string& a : runtimeIncludes) args.push_back(a);
        if (positionIndependent) args.push_back("-fPIC");
        args.push_back("-c");
        args.push_back(cppPath.string());
        args.push_back("-o");
        args.push_back(objPath.string());
    }
    return args;
}

/// Echoes `args` to stderr, one token per line prefixed "+ ", when
/// `verbose` - see Options::verbose's own doc comment for why this exists.
void echoIfVerbose(const std::vector<std::string>& args, bool verbose) {
    if (!verbose) return;
    std::cerr << "+";
    for (const std::string& a : args) std::cerr << " " << a;
    std::cerr << "\n";
}

/// Runs a backend-compiler invocation that involves actually compiling a
/// source file (as opposed to a pure archive/link-only step), routing
/// through `runProcessCaptureOutput` (discarding stdout on success, only
/// echoing it via stderr on failure) rather than plain `runProcess` when
/// `msvc` is set. `cl.exe` unconditionally echoes each source filename it
/// compiles to stdout with no documented flag to suppress it - unlike
/// GCC/Clang, which stay silent on a successful compile - which would
/// otherwise leak into whatever the caller (a test harness, `ebpm run`)
/// treats as "the compiled program's real stdout" the moment that same
/// stream later runs the freshly built binary too - a real, confirmed
/// failure mode (an extra "foo.gen.cpp" line ahead of a package's actual
/// printed output). `runProcessCaptureOutput` only captures the child's
/// stdout - its stderr (where GCC/Clang's own diagnostics normally live)
/// already flows straight through unchanged either way; MSVC diagnostics
/// go to stdout instead, so re-printing the captured output on failure
/// keeps them visible too, not just discarded alongside the filename
/// echo.
int runCompilerStep(const std::vector<std::string>& args, bool msvc, bool verbose = false) {
    echoIfVerbose(args, verbose);
    if (!msvc) return ebasic::runProcess(ebasic::hostExecArgs(args));
    std::string output;
    int rc = ebasic::runProcessCaptureOutput(ebasic::hostExecArgs(args), output);
    if (rc != 0) std::cerr << output;
    return rc;
}

/// MSVC PCH-mismatch resilience: unlike GCC's own silent, graceful
/// fallback when a stale/mismatched .gch simply isn't used (see
/// runtimeIncludeArgs's own doc comment), a stale/mismatched MSVC .pch is
/// a *hard* compile error - C1852 ("... is not a valid precompiled header
/// file") is the one actually confirmed live, against a real cl.exe, by
/// deliberately corrupting a built .pch (tests/cli/msvc_pch_fallback.sh);
/// C1010 ("unexpected end of file while looking for precompiled header
/// directive"), C1083 ("cannot open precompiled header file"), and C2859
/// ("... does not match the precompiled header") are Microsoft's other
/// documented codes for the same family of failure (a missing file, a
/// version/flag mismatch, ...) - kept as a defensive superset even though
/// only C1852 has been reproduced here. This reproduces the same "always
/// correct, just slower if unavailable"
/// guarantee explicitly, since MSVC won't do it on its own. Skips the
/// extra captured-output round trip entirely when `firstArgs` never
/// actually included a `/Yu` flag in the first place (the ordinary case
/// on a machine with no working MSVC PCH build) - only a real PCH-flavored
/// failure pays for the retry. `rebuildArgsWithoutPch` reruns the same
/// *Args() builder with `usePch=false` - PCH flags aren't guaranteed
/// contiguous/last in `firstArgs`, so this rebuilds from scratch rather
/// than trying to strip them back out.
int runCompilerStepWithPchFallback(const std::vector<std::string>& firstArgs, bool msvc,
                                    const std::function<std::vector<std::string>()>& rebuildArgsWithoutPch,
                                    bool verbose = false) {
    bool usedPch = msvc && std::any_of(firstArgs.begin(), firstArgs.end(), [](const std::string& a) {
                       return a.rfind("/Yu", 0) == 0;
                   });
    if (!usedPch) return runCompilerStep(firstArgs, msvc, verbose);

    echoIfVerbose(firstArgs, verbose);
    std::string output;
    int rc = ebasic::runProcessCaptureOutput(ebasic::hostExecArgs(firstArgs), output);
    if (rc == 0) return 0;
    bool pchMismatch = output.find("C1852") != std::string::npos ||
                        output.find("C1010") != std::string::npos ||
                        output.find("C1083") != std::string::npos ||
                        output.find("C2859") != std::string::npos;
    if (!pchMismatch) {
        std::cerr << output;
        return rc;
    }
    return runCompilerStep(rebuildArgsWithoutPch(), msvc, verbose);
}

/// Builds the "archive one object file into a static library" step -
/// `ar rcs` for GCC/Clang-family toolchains, MSVC's own librarian
/// (`lib.exe`) otherwise. The output filename is unconditionally
/// "lib<name>.a" regardless of toolchain (see main()'s --lib mode below) -
/// ebpm's own archivePath() (pkg/src/build.cpp) hardcodes exactly this
/// pattern on every platform already, and neither tool cares about the
/// ".a" extension beyond using it as a plain output filename.
std::vector<std::string> archiveArgs(const fs::path& archivePath, const fs::path& objPath, bool msvc) {
    if (msvc) {
        return {"lib", "/nologo", "/OUT:" + archivePath.string(), objPath.string()};
    }
    return {"ar", "rcs", archivePath.string(), objPath.string()};
}

/// Resolves one `-l`/`Lib "name"` entry to the token MSVC's linker should
/// see - two different naming conventions collide in the same `libNames`
/// list, and MSVC (unlike GNU `-l`, which auto-tries both "lib<name>.a"
/// and "<name>.lib" on Windows already) needs the exact filename: ebc's
/// own `--lib`-built archives are unconditionally named "lib<name>.a"
/// regardless of toolchain (see `archiveArgs` above - so a multi-package
/// ebpm build never needs to change based on platform), while a CMake- or
/// hand-built library (a test fixture, or eventually a real system import
/// lib like `User32.lib`) keeps its own toolchain-native name with no
/// "lib" prefix. Checked against the actual `-L`/`libDirs` search
/// directories rather than guessed, since ebc has no other way to know
/// which convention a given name follows; a name found in neither form
/// there falls through to the bare "<name>.lib" token so MSVC's own
/// default LIB search path (SDK/CRT directories) still resolves a real
/// system library exactly like it would today.
std::string resolveMsvcLibToken(const std::string& name, const std::vector<std::string>& libDirs) {
    std::error_code ec;
    for (const std::string& dir : libDirs) {
        if (fs::exists(fs::path(dir) / ("lib" + name + ".a"), ec)) return "lib" + name + ".a";
        if (fs::exists(fs::path(dir) / (name + ".lib"), ec)) return name + ".lib";
    }
    return name + ".lib";
}

/// Builds the "link one object file into a real shared library" step -
/// this is where the three GCC/Clang-family shapes (ELF `-shared`,
/// Mach-O `-dynamiclib`, MinGW's PE+import-library convention) and MSVC's
/// own shape (the compile driver's `/LD`, plus a `/link`-prefixed section
/// forwarded verbatim to `link.exe` for `/IMPLIB`, `/LIBPATH`, and plain
/// "<name>.lib" library tokens - `/IMPLIB` has no `cl.exe`-level spelling,
/// so it must go through `/link`) all live. `libNames` is the merged,
/// already-ordered `codegen.externLibs()` + `opts.extraLibNames` list -
/// order doesn't matter to MSVC's linker, but keeping it identical to the
/// GCC/Clang path's own traditional-linker-safe ordering costs nothing.
std::vector<std::string> sharedLinkArgs(const std::string& cxx, bool msvc, const std::string& targetOS,
                                         const fs::path& objPath, const fs::path& sharedLibPath,
                                         const fs::path& importLibPath,
                                         const std::vector<std::string>& libDirs,
                                         const std::vector<std::string>& libNames,
                                         const fs::path& msvcPchObject = {}) {
    if (msvc) {
        std::vector<std::string> args = {cxx, "/nologo", objPath.string()};
        /// See msvcRuntimePchObjectPath's own doc comment - only appended
        /// when the earlier compile-to-object step actually used the PCH,
        /// so this is a real, existing file whenever present.
        if (!msvcPchObject.empty()) args.push_back(msvcPchObject.string());
        args.push_back("/LD");
        args.push_back("/Fe:" + sharedLibPath.string());
        args.push_back("/link");
        args.push_back("/IMPLIB:" + importLibPath.string());
        for (const std::string& dir : libDirs) args.push_back("/LIBPATH:" + dir);
        for (const std::string& lib : libNames) args.push_back(resolveMsvcLibToken(lib, libDirs));
        return args;
    }
    bool isMacos = targetOS == "macos";
    bool isWindows = targetOS == "windows";
    std::vector<std::string> args = {cxx};
    args.push_back(isMacos ? "-dynamiclib" : "-shared");
    if (!isWindows) args.push_back("-fPIC");
    args.push_back(objPath.string());
    args.push_back("-o");
    args.push_back(sharedLibPath.string());
    if (isWindows) args.push_back("-Wl,--out-implib," + importLibPath.string());
    for (const std::string& dir : libDirs) {
        args.push_back("-L");
        args.push_back(dir);
    }
    for (const std::string& lib : libNames) args.push_back("-l" + lib);
    return args;
}

/// Builds the plain-executable mode's single combined compile+link step -
/// MSVC's `cl.exe` still does this in one invocation the same way g++/
/// clang++ do, just with `/`-style flags and, when there's anything to
/// link beyond the runtime, a `/link`-prefixed section forwarded to
/// `link.exe` for `/LIBPATH`/"<name>.lib" tokens (mirroring
/// `sharedLinkArgs` above).
std::vector<std::string> exeCompileLinkArgs(const std::string& cxx, bool msvc,
                                             const std::vector<std::string>& runtimeIncludes,
                                             const fs::path& cppPath, const std::string& outputPath,
                                             const std::vector<std::string>& libDirs,
                                             const std::vector<std::string>& libNames,
                                             const fs::path& pchObjectToLink = {}) {
    std::vector<std::string> args = {cxx};
    if (msvc) {
        args.push_back("/nologo");
        args.push_back("/std:c++17");
        args.push_back("/EHsc");
        for (const std::string& a : runtimeIncludes) args.push_back(a == "-I" ? "/I" : a);
        args.push_back(cppPath.string());
        /// See msvcRuntimePchObjectIfAvailable's own doc comment - the
        /// caller computes this independently of `runtimeIncludes` above
        /// (which may reflect a PCH-less retry for *this* file, or simply
        /// not use PCH at all) since a linked static archive might still
        /// need it.
        if (!pchObjectToLink.empty()) args.push_back(pchObjectToLink.string());
        args.push_back("/Fe:" + outputPath);
        if (!libDirs.empty() || !libNames.empty()) {
            args.push_back("/link");
            for (const std::string& dir : libDirs) args.push_back("/LIBPATH:" + dir);
            for (const std::string& lib : libNames) args.push_back(resolveMsvcLibToken(lib, libDirs));
        }
        return args;
    }
    args.push_back("-std=c++17");
    for (const std::string& a : runtimeIncludes) args.push_back(a);
    args.push_back(cppPath.string());
    args.push_back("-o");
    args.push_back(outputPath);
    /// Library names from `Lib "name"` clauses (M4) - -l flags must come
    /// after the object/source files on the command line for a
    /// traditional (non-`--start-group`) linker to resolve symbols from
    /// them correctly.
    for (const std::string& dir : libDirs) {
        args.push_back("-L");
        args.push_back(dir);
    }
    for (const std::string& lib : libNames) args.push_back("-l" + lib);
    return args;
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    std::string err;
    if (!parseArgs(argc, argv, opts, err)) {
        std::cerr << "ebc: error: " << err << "\n";
        printUsage(std::cerr);
        return 1;
    }
    if (opts.showVersion) {
        std::cout << "ebc " << ebasic::versionString() << "\n";
        return 0;
    }
    if (opts.showHelp) {
        printUsage(std::cout);
        return 0;
    }

    std::ifstream in(opts.inputPath);
    if (!in) {
        std::cerr << "ebc: error: cannot open input file: " << opts.inputPath << "\n";
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string rawSource = buf.str();

    ebasic::DiagnosticEngine diags;
    diags.registerFile(opts.inputPath); // fileId 0

    ebasic::PreprocessResult preprocessed =
        ebasic::preprocess(rawSource, opts.inputPath, diags, opts.includeDirs);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Lexer lexer(preprocessed.source, preprocessed.lineMap, diags);
    auto tokens = lexer.tokenize();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Parser parser(std::move(tokens), diags);
    ebasic::Module module = parser.parseModule();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Sema sema(diags);
    sema.check(module);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    if (opts.libMode || opts.sharedLibMode) {
        std::string err;
        if (!hasOnlyLibDeclarations(module, err)) {
            std::cerr << "ebc: error: " << err << "\n";
            return 1;
        }
    }

    ebasic::Codegen codegen;
    std::string cpp = codegen.generate(module, opts.libMode || opts.sharedLibMode, opts.sharedLibMode);

    if (opts.outputPath.empty()) {
        opts.outputPath = fs::path(opts.inputPath).stem().string();
    }

    fs::path cppPath = opts.outputPath + ".gen.cpp";
    {
        std::ofstream out(cppPath);
        out << cpp;
    }

    std::string cxx = opts.cxx;
    if (cxx.empty()) {
        const char* envCxx = std::getenv("CXX");
        if (envCxx) {
            cxx = envCxx;
        } else {
            /// No explicit backend named - default to whichever family
            /// built *this* ebc binary, not unconditionally "g++": an
            /// MSVC-built ebc (_MSC_VER) defaults to "cl" rather than
            /// silently falling back to whatever g++ happens to be on
            /// PATH, which would then try to link this build's own
            /// MSVC-format static libs (e.g. a fixture archive built by
            /// the same CMake preset) with GNU ld - a real, confirmed
            /// failure mode ("corrupt .drectve", "ld returned N exit
            /// status") distinct from anything about the .bas source
            /// itself. Every other build of ebc (g++, clang++, and any
            /// future clang-cl build too, since __clang__ without
            /// _MSC_VER still means a GCC/Clang-flag-shaped driver) keeps
            /// today's plain "g++" default, unchanged.
#ifdef _MSC_VER
            cxx = "cl";
#else
            cxx = "g++";
#endif
        }
    }
    bool msvc = isMsvcToolchain(cxx);

    int rc = 0;
    if (opts.libMode) {
        /// M5: compile to an object file only (no main() to link into an
        /// executable - genuinely absent, see Codegen::generate's libMode),
        /// then archive it into a static lib alongside an auto-generated
        /// interface .bas file, exactly mirroring the M4 fixture-library
        /// pattern (transpile -> compile -> ar), just driven by ebc itself.
        fs::path outDir = fs::path(opts.outputPath).parent_path();
        std::string libName = fs::path(opts.outputPath).filename().string();
        fs::path objPath = opts.outputPath + ".o";
        fs::path archivePath = outDir / ("lib" + libName + ".a");
        fs::path ifacePath = outDir / (libName + ".iface.bas");
        /// Sidecar file (M5c fast-follow): the archive's own `Lib "name"`
        /// clauses (M4), one per line - `.iface.bas`'s own `Extern "C++"`
        /// block only ever names *this* package's own archive, never a raw
        /// system library a package's Extern declarations (rather than a
        /// bodied SUB/FUNCTION) merely re-declare, so that information would
        /// otherwise be lost the moment a *different* package `ebpm build`s
        /// against this one - see pkg/src/build.cpp's own use of this file
        /// for why a downstream consumer needs it forwarded transitively.
        fs::path libsPath = outDir / (libName + ".libs");

        /// MSVC PCH is used here too, same as every other mode - this
        /// archive's one object file is consumed by a *separate*, later
        /// `ebc` invocation, but that invocation's own exe/`--shared-lib`
        /// link step now defensively includes the PCH-creation object
        /// whenever one exists (see msvcRuntimePchObjectIfAvailable's own
        /// doc comment), regardless of whether *that* invocation's own
        /// compile needed PCH - so the transitive `/Yu` requirement this
        /// object may carry is satisfied there, not here. `lib.exe`
        /// itself never links (no `LNK2011` risk in *this* invocation),
        /// so no PCH-object handling is needed for the archiving step
        /// below.
        std::vector<std::string> runtimeIncludes = runtimeIncludeArgs(argv[0], msvc);
        std::vector<std::string> compileArgs =
            compileToObjectArgs(cxx, msvc, runtimeIncludes, cppPath, objPath, /*positionIndependent=*/false);
        rc = runCompilerStepWithPchFallback(
            compileArgs, msvc,
            [&]() {
                return compileToObjectArgs(cxx, msvc, runtimeIncludeArgs(argv[0], msvc, /*usePch=*/false), cppPath,
                                            objPath, /*positionIndependent=*/false);
            },
            opts.verbose);
        if (rc == 0) {
            rc = ebasic::runProcess(ebasic::hostExecArgs(archiveArgs(archivePath, objPath, msvc)));
        }
        if (rc == 0) {
            std::ofstream ifaceOut(ifacePath);
            ifaceOut << codegen.generateLibraryInterface(module, libName);
            std::ofstream libsOut(libsPath);
            for (const std::string& lib : codegen.externLibs()) libsOut << lib << "\n";
        }
        std::error_code ec;
        fs::remove(objPath, ec);
    } else if (opts.sharedLibMode) {
        /// Real, dynamically loadable shared library - unlike --lib's static
        /// archive, this needs a genuine platform-specific link step (shared
        /// object/dylib/DLL), not `ar`. Only an `isExported` procedure
        /// (real body inside `Extern "C" ... End Extern`) gets a stable,
        /// unmangled, dllexport/visibility-default symbol - see
        /// Codegen::genProcedure. The same .iface.bas/.libs sidecar files
        /// --lib produces are generated too (orthogonal to archiving-vs-
        /// shared-linking: this package can still also be depended on by
        /// another eBasic package via its ordinary mangled symbols, not just
        /// its opt-in C exports).
        fs::path outDir = fs::path(opts.outputPath).parent_path();
        std::string libName = fs::path(opts.outputPath).filename().string();
        fs::path objPath = opts.outputPath + ".o";
        fs::path ifacePath = outDir / (libName + ".iface.bas");
        fs::path libsPath = outDir / (libName + ".libs");

        std::string targetOS = currentTargetOS();
        bool isWindows = targetOS == "windows";
        bool isMacos = targetOS == "macos";
        std::string prefix = isWindows ? "" : "lib";
        std::string ext = isWindows ? ".dll" : (isMacos ? ".dylib" : ".so");
        fs::path sharedLibPath = outDir / (prefix + libName + ext);
        /// MinGW's own convention: another program links against this import
        /// library at build time (there is no real static archive of the
        /// DLL's code to link against directly, unlike ELF/Mach-O where the
        /// shared object itself is linked against).
        fs::path importLibPath = outDir / ("lib" + libName + ".dll.a");

        std::vector<std::string> runtimeIncludes = runtimeIncludeArgs(argv[0], msvc);
        std::vector<std::string> compileArgs =
            compileToObjectArgs(cxx, msvc, runtimeIncludes, cppPath, objPath, /*positionIndependent=*/!isWindows);
        rc = runCompilerStepWithPchFallback(
            compileArgs, msvc,
            [&]() {
                return compileToObjectArgs(cxx, msvc, runtimeIncludeArgs(argv[0], msvc, /*usePch=*/false), cppPath,
                                            objPath, /*positionIndependent=*/!isWindows);
            },
            opts.verbose);

        if (rc == 0) {
            std::vector<std::string> libNames = codegen.externLibs();
            for (const std::string& lib : opts.extraLibNames) libNames.push_back(lib);
            /// See msvcRuntimePchObjectIfAvailable's own doc comment -
            /// computed independently of whether the compile above itself
            /// used PCH, since a linked static archive (opts.libDirs/
            /// libNames) might need it regardless.
            std::vector<std::string> linkArgs =
                sharedLinkArgs(cxx, msvc, targetOS, objPath, sharedLibPath, importLibPath, opts.libDirs, libNames,
                               msvcRuntimePchObjectIfAvailable(argv[0], msvc));
            rc = ebasic::runProcess(ebasic::hostExecArgs(linkArgs));
        }
        if (rc == 0) {
            std::ofstream ifaceOut(ifacePath);
            ifaceOut << codegen.generateLibraryInterface(module, libName);
            std::ofstream libsOut(libsPath);
            for (const std::string& lib : codegen.externLibs()) libsOut << lib << "\n";
        }
        std::error_code ec;
        fs::remove(objPath, ec);
    } else {
        /// Library names from `Lib "name"` clauses (M4), plus M5c's own
        /// explicit -l names (a transitive dependency's library, whose own
        /// Lib clause never appears in *this* module at all - see
        /// Options::extraLibNames's doc comment).
        std::vector<std::string> libNames = codegen.externLibs();
        for (const std::string& lib : opts.extraLibNames) libNames.push_back(lib);
        /// Computed once, independent of whichever runtimeIncludes variant
        /// (with or without PCH) actually ends up compiling this file -
        /// see msvcRuntimePchObjectIfAvailable's own doc comment: a linked
        /// static archive (opts.libDirs/libNames, e.g. from an earlier
        /// `ebc --lib` build) may need this object even when this
        /// invocation's own compile doesn't.
        fs::path pchObjectToLink = msvcRuntimePchObjectIfAvailable(argv[0], msvc);
        std::vector<std::string> compileArgs =
            exeCompileLinkArgs(cxx, msvc, runtimeIncludeArgs(argv[0], msvc), cppPath, opts.outputPath, opts.libDirs,
                                libNames, pchObjectToLink);
        rc = runCompilerStepWithPchFallback(
            compileArgs, msvc,
            [&]() {
                return exeCompileLinkArgs(cxx, msvc, runtimeIncludeArgs(argv[0], msvc, /*usePch=*/false), cppPath,
                                           opts.outputPath, opts.libDirs, libNames, pchObjectToLink);
            },
            opts.verbose);
    }

    if (!opts.keepCpp) {
        std::error_code ec;
        fs::remove(cppPath, ec);
    }

    if (rc != 0) {
        std::cerr << "ebc: error: backend compilation failed (exit code " << rc << ")\n";
        return 1;
    }

    return 0;
}
