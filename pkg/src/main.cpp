#include "build.hpp"
#include "driver/process.hpp"
#include "manifest.hpp"
#include "resolve.hpp"

#include "ebasic/version.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void printUsage(std::ostream& os) {
    os << "usage: ebpm <command> [args]\n";
    os << "       ebpm [-v | --version] [-h | --help]\n";
    os << "commands:\n";
    os << "  new <name> [--lib]   scaffold a new package directory <name>/\n";
    os << "  init [--lib]         scaffold a package in the current directory\n";
    os << "  build                build the package in the current directory\n";
    os << "  run [-- args...]     build (if stale), then run the [bin] target\n";
    os << "  test                 build+run every tests/*.bas as a standalone\n";
    os << "                       program; pass = exit code 0\n";
}

/// Loads "ebasic.toml" from the current directory - every command below
/// operates on the package rooted at cwd (M5b: no dependency graph, no
/// package-path argument yet).
bool loadCurrentManifest(ebpm::Manifest& out, std::string& err) {
    return ebpm::loadManifest("ebasic.toml", out, err);
}

/// Bare package-name text for a manifest's [package] name = "..." line - not
/// a general TOML string escaper (a package name is always a plain
/// identifier-ish string in practice), just enough to avoid producing
/// malformed TOML from a name containing a literal quote.
std::string escapeTomlString(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') r.push_back('\\');
        r.push_back(c);
    }
    return r;
}

std::string manifestText(const std::string& name, bool isLib) {
    std::ostringstream out;
    out << "[package]\n";
    out << "name = \"" << escapeTomlString(name) << "\"\n";
    out << "version = \"0.1.0\"\n\n";
    if (isLib) {
        out << "[lib]\n";
        out << "path = \"src/lib.bas\"\n";
    } else {
        out << "[bin]\n";
        out << "name = \"" << escapeTomlString(name) << "\"\n";
        out << "path = \"src/main.bas\"\n";
    }
    return out.str();
}

std::string stubBinSource(const std::string& name) {
    return "PRINT \"Hello from " + name + "!\"\n";
}

std::string stubLibSource() {
    return "FUNCTION Placeholder() AS INTEGER\n    Placeholder = 0\nEND FUNCTION\n";
}

/// Writes a fresh package (manifest + one stub source file) into `dir`,
/// which must already exist and be empty of any prior `ebasic.toml`. Shared
/// by `new` (dir is a just-created subdirectory) and `init` (dir is `.`).
bool scaffoldInto(const fs::path& dir, const std::string& name, bool isLib, std::string& err) {
    fs::path manifestPath = dir / "ebasic.toml";
    if (fs::exists(manifestPath)) {
        err = manifestPath.string() + " already exists";
        return false;
    }
    std::error_code ec;
    fs::create_directories(dir / "src", ec);
    if (ec) {
        err = "could not create '" + (dir / "src").string() + "': " + ec.message();
        return false;
    }

    {
        std::ofstream out(manifestPath);
        out << manifestText(name, isLib);
    }
    fs::path srcPath = dir / (isLib ? "src/lib.bas" : "src/main.bas");
    if (!fs::exists(srcPath)) {
        std::ofstream out(srcPath);
        out << (isLib ? stubLibSource() : stubBinSource(name));
    }
    return true;
}

int cmdNew(const std::vector<std::string>& args) {
    bool isLib = false;
    std::string name;
    for (const std::string& a : args) {
        if (a == "--lib") isLib = true;
        else if (!name.empty()) {
            std::cerr << "ebpm: error: multiple package names given\n";
            return 1;
        } else {
            name = a;
        }
    }
    if (name.empty()) {
        std::cerr << "ebpm: error: 'new' requires a package name\n";
        return 1;
    }
    fs::path dir = name;
    if (fs::exists(dir)) {
        std::cerr << "ebpm: error: '" << dir.string() << "' already exists\n";
        return 1;
    }
    std::string err;
    if (!scaffoldInto(dir, name, isLib, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Created " << (isLib ? "library" : "binary") << " package '" << name << "' at "
              << dir.string() << "\n";
    return 0;
}

int cmdInit(const std::vector<std::string>& args) {
    bool isLib = false;
    for (const std::string& a : args) {
        if (a == "--lib") isLib = true;
        else {
            std::cerr << "ebpm: error: unknown argument to 'init': " << a << "\n";
            return 1;
        }
    }
    fs::path dir = fs::current_path();
    std::string name = dir.filename().string();
    std::string err;
    if (!scaffoldInto(dir, name, isLib, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Created " << (isLib ? "library" : "binary") << " package '" << name
              << "' in the current directory\n";
    return 0;
}

int cmdBuild(const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::cerr << "ebpm: error: 'build' takes no arguments yet\n";
        return 1;
    }
    std::string err;
    int rc = ebpm::buildPackageWithDeps(".", err);
    if (!err.empty()) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    return rc;
}

int cmdRun(const std::vector<std::string>& args) {
    /// Everything before a literal "--" would be an ebpm-side run option -
    /// none exist yet, so any argument not after "--" is an error. Everything
    /// after "--" is forwarded verbatim to the built binary.
    std::vector<std::string> forwardArgs;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--") {
            forwardArgs.assign(args.begin() + static_cast<long>(i) + 1, args.end());
            break;
        }
        std::cerr << "ebpm: error: unknown argument to 'run': " << args[i]
                   << " (forward program arguments after --)\n";
        return 1;
    }

    ebpm::Manifest manifest;
    std::string err;
    if (!loadCurrentManifest(manifest, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (!manifest.hasBin) {
        std::cerr << "ebpm: error: package '" << manifest.name << "' has no [bin] target to run\n";
        return 1;
    }

    /// Staleness is checked against every package in the resolved graph, not
    /// just the root's own source - a dependency's source changing must
    /// also trigger a rebuild of whatever depends on it.
    std::vector<ebpm::ResolvedPackage> order;
    if (!ebpm::resolveDependencyGraph(".", order, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::string binPath = ebpm::binaryPath(manifest, ".");
    std::vector<std::string> srcPaths;
    for (const ebpm::ResolvedPackage& pkg : order) {
        if (pkg.manifest.hasLib) srcPaths.push_back((fs::path(pkg.dir) / pkg.manifest.lib.path).string());
        if (pkg.manifest.hasBin) srcPaths.push_back((fs::path(pkg.dir) / pkg.manifest.bin.path).string());
    }
    if (ebpm::isStale(srcPaths, binPath)) {
        int rc = ebpm::buildPackageWithDeps(".", err);
        if (!err.empty()) {
            std::cerr << "ebpm: error: " << err << "\n";
            return 1;
        }
        if (rc != 0) return rc;
    }

    std::vector<std::string> runArgs = {binPath};
    for (const std::string& a : forwardArgs) runArgs.push_back(a);
    return ebasic::runProcess(runArgs);
}

/// M5e: the eBasic *language* has no test/assert syntax yet, so this is
/// deliberately the simplest meaningful interpretation of "a test" - a
/// standalone `.bas` program under tests/, compiled and run as a *consumer*
/// of the package under test (able to #include/link its interface exactly
/// like an external dependent package would - see computeConsumerDirs), and
/// judged purely by its own exit code. No assertion primitives, no
/// golden-output diffing - both would need real language support first, and
/// are easy to layer on top of this once/if that exists.
int cmdTest(const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::cerr << "ebpm: error: 'test' takes no arguments yet\n";
        return 1;
    }

    std::string err;
    /// Build the package (and its dependencies) first, so a [lib] package's
    /// own interface/archive already exist for a test file to use.
    int rc = ebpm::buildPackageWithDeps(".", err);
    if (!err.empty()) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (rc != 0) return rc;

    fs::path testsDir = "tests";
    if (!fs::exists(testsDir) || !fs::is_directory(testsDir)) {
        std::cout << "no tests/ directory found - nothing to test\n";
        return 0;
    }

    std::vector<std::string> includeDirs, libDirs, libNames;
    if (!ebpm::computeConsumerDirs(".", includeDirs, libDirs, libNames, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }

    std::vector<std::string> testFiles;
    for (const auto& entry : fs::directory_iterator(testsDir)) {
        if (entry.path().extension() == ".bas") testFiles.push_back(entry.path().string());
    }
    std::sort(testFiles.begin(), testFiles.end());

    if (testFiles.empty()) {
        std::cout << "no tests/*.bas files found - nothing to test\n";
        return 0;
    }

    fs::path testOutDir = fs::path("target") / "tests";
    std::error_code ec;
    fs::create_directories(testOutDir, ec);

    std::string ebc = ebpm::ebcCommand();
    int passed = 0;
    int failed = 0;
    for (const std::string& testFile : testFiles) {
        std::string stem = fs::path(testFile).stem().string();
        std::string outPath = (testOutDir / stem).string();

        std::vector<std::string> compileArgs = {ebc, testFile, "-o", outPath};
        for (const std::string& dir : includeDirs) {
            compileArgs.push_back("-I");
            compileArgs.push_back(dir);
        }
        for (const std::string& dir : libDirs) {
            compileArgs.push_back("-L");
            compileArgs.push_back(dir);
        }
        for (const std::string& lib : libNames) {
            compileArgs.push_back("-l");
            compileArgs.push_back(lib);
        }

        if (ebasic::runProcess(compileArgs) != 0) {
            std::cout << "test " << stem << " ... FAILED (did not compile)\n";
            ++failed;
            continue;
        }
        int testRc = ebasic::runProcess({outPath});
        if (testRc == 0) {
            std::cout << "test " << stem << " ... ok\n";
            ++passed;
        } else {
            std::cout << "test " << stem << " ... FAILED (exit code " << testRc << ")\n";
            ++failed;
        }
    }

    std::cout << "\ntest result: " << (failed == 0 ? "ok" : "FAILED") << ". " << passed
              << " passed; " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        printUsage(std::cerr);
        return 1;
    }
    std::string command = args[0];
    /// -v/--version and -h/--help are checked on the leading token only, not
    /// scanned for anywhere in argv - "run" forwards everything after "--"
    /// to the user's own program (e.g. `ebpm run -- --help`), which must
    /// reach that program untouched, not be swallowed by ebpm itself.
    if (command == "-v" || command == "--version") {
        std::cout << "ebpm " << ebasic::versionString() << "\n";
        return 0;
    }
    if (command == "-h" || command == "--help") {
        printUsage(std::cout);
        return 0;
    }
    std::vector<std::string> rest(args.begin() + 1, args.end());

    if (command == "new") return cmdNew(rest);
    if (command == "init") return cmdInit(rest);
    if (command == "build") return cmdBuild(rest);
    if (command == "run") return cmdRun(rest);
    if (command == "test") return cmdTest(rest);

    std::cerr << "ebpm: error: unknown command '" << command << "'\n";
    printUsage(std::cerr);
    return 1;
}
