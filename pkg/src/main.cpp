#include "build.hpp"
#include "driver/process.hpp"
#include "index.hpp"
#include "lockfile.hpp"
#include "manifest.hpp"
#include "resolve.hpp"
#include "semver.hpp"

#include "ebasic/version.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

void printUsage(std::ostream& os) {
    os << "usage: ebpm <command> [args]\n";
    os << "       ebpm [-v | --version] [-h | --help]\n";
    os << "commands:\n";
    os << "  new <name> [--lib]        scaffold a new package directory <name>/\n";
    os << "  init [--lib]              scaffold a package in the current directory\n";
    os << "  build                     build the package in the current directory\n";
    os << "  run [-- args...]          build (if stale), then run the [bin] target\n";
    os << "  test                      build+run every tests/*.bas as a standalone\n";
    os << "                            program; pass = exit code 0\n";
    os << "  add <name> [--version <req>]\n";
    os << "                            look up <name> in the package index and add it\n";
    os << "                            to [dependencies] (highest version, or the\n";
    os << "                            highest satisfying <req> if given)\n";
    os << "  remove <name>             remove <name> from [dependencies]\n";
    os << "  list                      show the resolved dependency tree\n";
    os << "  search <term>             search the package index by name/description\n";
    os << "  update [<name>]           re-resolve a registry dependency (or all of\n";
    os << "                            them) to its current highest satisfying\n";
    os << "                            version, ignoring any existing lockfile pin\n";
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

/// Trims leading/trailing spaces/tabs/carriage-returns (this project's own
/// manifests are plain ASCII, no need for anything Unicode-aware).
std::string trimSpaces(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> readLines(const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "could not read '" + path + "'";
        return {};
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

bool writeLines(const std::string& path, const std::vector<std::string>& lines, std::string& err) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        err = "could not write '" + path + "'";
        return false;
    }
    for (const std::string& line : lines) out << line << "\n";
    return true;
}

/// REG-6: `name` already a dependency anywhere in the manifest - checked
/// across `manifest.dependencies` *and* every `[target.*.dependencies]`
/// section, not just `effectiveDependencies()` (which only reflects the
/// *current* platform - a name could exist only in a different platform's
/// section).
bool dependencyNameExists(const ebpm::Manifest& manifest, const std::string& name) {
    for (const ebpm::Dependency& d : manifest.dependencies) {
        if (d.name == name) return true;
    }
    for (const auto& [os, deps] : manifest.targetDependencies) {
        for (const ebpm::Dependency& d : deps) {
            if (d.name == name) return true;
        }
    }
    return false;
}

/// REG-7: whether `name` is specifically a *registry* ("version")
/// dependency somewhere in the manifest - `ebpm update <name>` only makes
/// sense for one of these (a `path`/`git` dependency has no index-picked
/// version to re-pick), so this drives that command's own validation.
bool isRegistryDependency(const ebpm::Manifest& manifest, const std::string& name) {
    for (const ebpm::Dependency& d : manifest.dependencies) {
        if (d.name == name && !d.version.empty()) return true;
    }
    for (const auto& [os, deps] : manifest.targetDependencies) {
        for (const ebpm::Dependency& d : deps) {
            if (d.name == name && !d.version.empty()) return true;
        }
    }
    return false;
}

/// Every distinct registry dependency name in the manifest, across
/// `[dependencies]` and every `[target.*.dependencies]` section - what
/// `ebpm update` (with no name given) updates all of.
std::vector<std::string> registryDependencyNames(const ebpm::Manifest& manifest) {
    std::vector<std::string> names;
    auto add = [&](const ebpm::Dependency& d) {
        if (d.version.empty()) return;
        if (std::find(names.begin(), names.end(), d.name) == names.end()) names.push_back(d.name);
    };
    for (const ebpm::Dependency& d : manifest.dependencies) add(d);
    for (const auto& [os, deps] : manifest.targetDependencies) {
        for (const ebpm::Dependency& d : deps) add(d);
    }
    return names;
}

/// Deletes each named package's whole `[[package]]` block from
/// `ebasic.lock` (including the blank line `writeLockfile` always emits
/// before one), rather than rewriting it via `readLockfilePins` +
/// `writeLockfile` - the latter needs a full, freshly-resolved
/// `ResolvedPackage`, which is exactly what doesn't exist yet: the whole
/// point is to make the *next* `resolveDependencyGraph` call see no pin at
/// all for these names, forcing a fresh index lookup instead of reusing a
/// stale pin - exactly as if they'd never been built before. A no-op (not
/// an error) if the lockfile doesn't exist yet.
bool removeLockfilePinBlocks(const std::string& path, const std::vector<std::string>& names,
                             std::string& err) {
    if (!fs::exists(path)) return true;
    std::vector<std::string> lines = readLines(path, err);
    if (!err.empty()) return false;

    std::vector<std::string> kept;
    size_t i = 0;
    while (i < lines.size()) {
        if (trimSpaces(lines[i]) != "[[package]]") {
            kept.push_back(lines[i]);
            ++i;
            continue;
        }
        size_t blockStart = i;
        size_t j = i + 1;
        std::string blockName;
        while (j < lines.size() && trimSpaces(lines[j]) != "[[package]]") {
            std::string t = trimSpaces(lines[j]);
            if (t.rfind("name = \"", 0) == 0 && t.size() >= 9 && t.back() == '"') {
                blockName = t.substr(8, t.size() - 9);
            }
            ++j;
        }
        if (std::find(names.begin(), names.end(), blockName) != names.end()) {
            if (!kept.empty() && kept.back().empty()) kept.pop_back();
        } else {
            for (size_t k = blockStart; k < j; ++k) kept.push_back(lines[k]);
        }
        i = j;
    }
    return writeLines(path, kept, err);
}

/// Inserts `line` as the [dependencies] section's first entry - a
/// text-level edit, not a parse+reserialize round-trip (toml++ is
/// read-only in this codebase, and a round-trip could reformat or drop a
/// user's own comments/formatting). Appends a brand new `[dependencies]`
/// section at EOF if none exists yet - the *common* case, since `ebpm
/// new`'s own scaffolded manifest never emits one.
bool insertDependencyLine(const std::string& manifestPath, const std::string& line, std::string& err) {
    std::vector<std::string> lines = readLines(manifestPath, err);
    if (!err.empty()) return false;

    int depsIndex = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (trimSpaces(lines[i]) == "[dependencies]") {
            depsIndex = static_cast<int>(i);
            break;
        }
    }
    if (depsIndex < 0) {
        lines.push_back("");
        lines.push_back("[dependencies]");
        lines.push_back(line);
    } else {
        lines.insert(lines.begin() + depsIndex + 1, line);
    }
    return writeLines(manifestPath, lines, err);
}

/// Removes the single-line `name = ...` entry from the manifest's
/// `[dependencies]` section - deliberately scoped to the simple
/// single-line shorthand form `add` itself always writes (`ebpm add`
/// never emits a multi-line inline table); a hand-written multi-line entry
/// is left untouched with an error, rather than risk corrupting the file.
bool removeDependencyLine(const std::string& manifestPath, const std::string& name, std::string& err) {
    std::vector<std::string> lines = readLines(manifestPath, err);
    if (!err.empty()) return false;

    int depsIndex = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (trimSpaces(lines[i]) == "[dependencies]") {
            depsIndex = static_cast<int>(i);
            break;
        }
    }
    if (depsIndex < 0) {
        err = "'" + name + "' is not a dependency (no [dependencies] section at all)";
        return false;
    }

    int foundIndex = -1;
    for (size_t i = depsIndex + 1; i < lines.size(); ++i) {
        std::string t = trimSpaces(lines[i]);
        if (!t.empty() && t.front() == '[') break; // the next section - [dependencies] has ended
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        if (trimSpaces(t.substr(0, eq)) == name) {
            foundIndex = static_cast<int>(i);
            break;
        }
    }
    if (foundIndex < 0) {
        err = "'" + name + "' is not a dependency in [dependencies]";
        return false;
    }
    long braceBalance = std::count(lines[foundIndex].begin(), lines[foundIndex].end(), '{') -
                         std::count(lines[foundIndex].begin(), lines[foundIndex].end(), '}');
    if (braceBalance != 0) {
        err = "'" + name +
              "'s dependency entry spans multiple lines - remove it by hand instead";
        return false;
    }

    lines.erase(lines.begin() + foundIndex);
    return writeLines(manifestPath, lines, err);
}

int cmdAdd(const std::vector<std::string>& args) {
    std::string name;
    std::string versionReqStr;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--version") {
            if (i + 1 >= args.size()) {
                std::cerr << "ebpm: error: --version requires an argument\n";
                return 1;
            }
            versionReqStr = args[++i];
        } else if (!name.empty()) {
            std::cerr << "ebpm: error: multiple package names given\n";
            return 1;
        } else {
            name = args[i];
        }
    }
    if (name.empty()) {
        std::cerr << "ebpm: error: 'add' requires a package name\n";
        return 1;
    }

    ebpm::Manifest manifest;
    std::string err;
    if (!loadCurrentManifest(manifest, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (dependencyNameExists(manifest, name)) {
        std::cerr << "ebpm: error: '" << name << "' is already a dependency - use `ebpm remove "
                  << name << "` first\n";
        return 1;
    }

    ebpm::PackageIndex pkgIndex;
    if (!ebpm::lookupPackage(name, pkgIndex, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (pkgIndex.versions.empty()) {
        std::cerr << "ebpm: error: '" << name << "' has no published versions in the index\n";
        return 1;
    }

    ebpm::SemVer chosen;
    if (!versionReqStr.empty()) {
        ebpm::VersionReq req;
        if (!ebpm::parseVersionReq(versionReqStr, req, err)) {
            std::cerr << "ebpm: error: " << err << "\n";
            return 1;
        }
        std::vector<ebpm::SemVer> available;
        for (const ebpm::IndexVersionEntry& v : pkgIndex.versions) available.push_back(v.version);
        auto pick = ebpm::pickBestSatisfying(available, req);
        if (!pick) {
            std::cerr << "ebpm: error: no version of '" << name << "' satisfies " << versionReqStr
                      << "\n";
            return 1;
        }
        chosen = *pick;
    } else {
        chosen = pkgIndex.versions.front().version;
        for (const ebpm::IndexVersionEntry& v : pkgIndex.versions) {
            if (v.version > chosen) chosen = v.version;
        }
    }

    std::string line = name + " = \"^" + ebpm::toString(chosen) + "\"";
    if (!insertDependencyLine("ebasic.toml", line, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Added " << name << " v" << ebpm::toString(chosen) << " to [dependencies]\n";
    return 0;
}

int cmdRemove(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cerr << "ebpm: error: 'remove' requires exactly one package name\n";
        return 1;
    }
    std::string err;
    if (!removeDependencyLine("ebasic.toml", args[0], err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Removed " << args[0] << " from [dependencies]\n";
    return 0;
}

/// REG-7: `ebpm list` - the *current project's* resolved dependency tree
/// (cargo-tree-style, but a flat annotated list rather than a nested ASCII
/// tree - bounds scope; see the plan's own REG-7 notes), reusing
/// `resolveDependencyGraph`'s already-computed `order` directly rather
/// than re-deriving anything. `order`'s own last entry is always the root
/// package itself (see resolveDependencyGraph's doc comment), so it's
/// skipped here - `list` shows dependencies, not the project itself.
int cmdList(const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::cerr << "ebpm: error: 'list' takes no arguments\n";
        return 1;
    }
    std::vector<ebpm::ResolvedPackage> order;
    std::string err;
    if (!ebpm::resolveDependencyGraph(".", order, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (order.size() <= 1) {
        std::cout << "no dependencies\n";
        return 0;
    }
    for (size_t i = 0; i + 1 < order.size(); ++i) {
        const ebpm::ResolvedPackage& pkg = order[i];
        std::string shortCommit = pkg.gitCommit.substr(0, std::min<size_t>(12, pkg.gitCommit.size()));
        std::cout << pkg.name;
        switch (pkg.sourceKind) {
            case ebpm::SourceKind::Path:
                std::cout << " (path: " << pkg.dir << ")";
                break;
            case ebpm::SourceKind::Git:
                std::cout << " (git, commit " << shortCommit << ")";
                break;
            case ebpm::SourceKind::Registry:
                std::cout << " v" << pkg.version << " (registry, commit " << shortCommit << ")";
                break;
            case ebpm::SourceKind::Root:
                break; // unreachable: only order's skipped last entry is ever Root
        }
        std::cout << "\n";
    }
    return 0;
}

/// REG-7: `ebpm search <term>` - browses the *index* (every published
/// package, regardless of whether the current project depends on it),
/// unlike `list` above - a substring match against name/description,
/// matching Cargo's own `cargo search` output shape (`name - description`).
int cmdSearch(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cerr << "ebpm: error: 'search' requires exactly one search term\n";
        return 1;
    }
    const std::string& term = args[0];
    std::vector<ebpm::PackageIndex> all;
    std::string err;
    if (!ebpm::listAllPackages(all, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    int matches = 0;
    for (const ebpm::PackageIndex& pkg : all) {
        if (pkg.name.find(term) != std::string::npos || pkg.description.find(term) != std::string::npos) {
            std::cout << pkg.name << " - " << pkg.description << "\n";
            ++matches;
        }
    }
    if (matches == 0) {
        std::cout << "no packages found matching '" << term << "'\n";
    }
    return 0;
}

/// REG-7: `ebpm update [<name>]` - the deliberate, explicit way to pick up
/// a newer compatible registry-dependency release, since an ordinary
/// `build`/`run`/`test` never re-consults the index once a version is
/// pinned (REG-5's whole point). Re-resolves the named registry dependency
/// (or every registry dependency, if no name given), ignoring its existing
/// lockfile pin, by deleting just that pin's `[[package]]` block up front -
/// `resolveDependencyGraph` then sees no pin at all for it and falls
/// through to a fresh index lookup exactly as if it had never been built.
int cmdUpdate(const std::vector<std::string>& args) {
    if (args.size() > 1) {
        std::cerr << "ebpm: error: 'update' takes at most one package name\n";
        return 1;
    }
    ebpm::Manifest manifest;
    std::string err;
    if (!loadCurrentManifest(manifest, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }

    std::vector<std::string> targets;
    if (!args.empty()) {
        if (!isRegistryDependency(manifest, args[0])) {
            std::cerr << "ebpm: error: '" << args[0] << "' is not a registry dependency\n";
            return 1;
        }
        targets.push_back(args[0]);
    } else {
        targets = registryDependencyNames(manifest);
        if (targets.empty()) {
            std::cout << "no registry dependencies to update\n";
            return 0;
        }
    }

    std::unordered_map<std::string, ebpm::Pin> oldPins = ebpm::readLockfilePins(".");
    if (!removeLockfilePinBlocks("ebasic.lock", targets, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }

    int rc = ebpm::buildPackageWithDeps(".", err);
    if (!err.empty()) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    if (rc != 0) return rc;

    std::unordered_map<std::string, ebpm::Pin> newPins = ebpm::readLockfilePins(".");
    for (const std::string& name : targets) {
        std::string oldVersion = oldPins.count(name) ? oldPins[name].version : "";
        std::string newVersion = newPins.count(name) ? newPins[name].version : "";
        if (!oldVersion.empty() && oldVersion == newVersion) {
            std::cout << name << " is already up to date (v" << newVersion << ")\n";
        } else if (oldVersion.empty()) {
            std::cout << "Locked " << name << " v" << newVersion << "\n";
        } else {
            std::cout << "Updating " << name << " v" << oldVersion << " -> v" << newVersion << "\n";
        }
    }
    return 0;
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
    if (command == "add") return cmdAdd(rest);
    if (command == "remove") return cmdRemove(rest);
    if (command == "list") return cmdList(rest);
    if (command == "search") return cmdSearch(rest);
    if (command == "update") return cmdUpdate(rest);

    std::cerr << "ebpm: error: unknown command '" << command << "'\n";
    printUsage(std::cerr);
    return 1;
}
