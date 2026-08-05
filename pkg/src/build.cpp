#include "build.hpp"
#include "driver/process.hpp"
#include "lockfile.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace ebpm {

namespace {

/// Reads `<targetDir>/<pkgName>.libs` (ebc --lib's sidecar of that
/// package's own `Lib "name"` clauses, one per line - see ebc's own
/// doc comment) if present, appending each into `out` (deduplicated
/// against what's already there). Absent for a dependency built by an
/// older ebc that predates this file, or one with no Extern Lib clauses
/// at all - either way, nothing to add, not an error.
void appendLibsSidecar(const std::string& targetDir, const std::string& pkgName,
                        std::vector<std::string>& out, std::unordered_set<std::string>& seen) {
    fs::path libsPath = fs::path(targetDir) / (pkgName + ".libs");
    std::ifstream in(libsPath);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        if (seen.insert(line).second) out.push_back(line);
    }
}

/// Splits a PATH-style, `:`-separated list of directories (empty segments
/// skipped) - the format `EBASIC_LIBRARY_PATH` below uses.
std::vector<std::string> splitPathList(const std::string& value) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= value.size()) {
        size_t pos = value.find(':', start);
        if (pos == std::string::npos) pos = value.size();
        if (pos > start) out.push_back(value.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

/// A real, external (non-ebpm) system library's own search directory - e.g.
/// wherever `libgtk-4` lives, or this project's own e2e fixtures' hand-built
/// `libebfixturec.a` - has no manifest/CLI representation today, only a
/// dependency *package*'s own target dir does (see the transitive-deps loop
/// below). `EBASIC_LIBRARY_PATH` (a `:`-separated list, read once here) is
/// the escape hatch: forwarded as `-L` to every `ebc` invocation, exactly
/// like a dependency's target dir already is. Deliberately a distinct,
/// ebasic-specific name, never the real `LIBRARY_PATH` - on Haiku, that
/// exact name is also consulted by the OS's own runtime_loader, and
/// pointing it at a directory holding only unrelated static archives
/// silently breaks *any* process on that platform, not just `ebc`
/// (confirmed by direct reproduction: even a bare `ls` crashed).
std::vector<std::string> externalLibraryDirs() {
    const char* envPath = std::getenv("EBASIC_LIBRARY_PATH");
    return envPath ? splitPathList(envPath) : std::vector<std::string>{};
}

} // namespace

std::string ebcCommand() {
    const char* envEbc = std::getenv("EBC");
    return envEbc ? envEbc : "ebc";
}

std::string binaryPath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / manifest.bin.name).string();
}

std::string archivePath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / ("lib" + manifest.name + ".a")).string();
}

std::string interfacePath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / (manifest.name + ".iface.bas")).string();
}

std::string sharedLibPath(const Manifest& manifest, const std::string& packageDir) {
#if defined(_WIN32)
    return (fs::path(packageDir) / "target" / (manifest.sharedLib.name + ".dll")).string();
#elif defined(__APPLE__)
    return (fs::path(packageDir) / "target" / ("lib" + manifest.sharedLib.name + ".dylib")).string();
#else
    return (fs::path(packageDir) / "target" / ("lib" + manifest.sharedLib.name + ".so")).string();
#endif
}

bool isStale(const std::vector<std::string>& srcPaths, const std::string& outPath) {
    std::error_code ec;
    if (!fs::exists(outPath, ec)) return true;
    auto outTime = fs::last_write_time(outPath, ec);
    if (ec) return true;
    for (const std::string& srcPath : srcPaths) {
        auto srcTime = fs::last_write_time(srcPath, ec);
        /// A missing source is a different problem (surfaced by ebc itself
        /// once the resulting rebuild attempt runs) - treated as stale here
        /// rather than silently skipped, so it can't mask a real problem
        /// (e.g. a dependency's source deleted out from under an otherwise
        /// still-newer binary) behind a false "nothing changed".
        if (ec || srcTime > outTime) return true;
    }
    return false;
}

int buildPackage(const Manifest& manifest, const std::string& packageDir,
                  const std::vector<std::string>& extraIncludeDirs,
                  const std::vector<std::string>& extraLibDirs,
                  const std::vector<std::string>& extraLibNames, std::string& err) {
    fs::path pkgDir(packageDir);
    fs::path targetDir = pkgDir / "target";
    std::error_code ec;
    fs::create_directories(targetDir, ec);
    if (ec) {
        err = "could not create '" + targetDir.string() + "': " + ec.message();
        return 1;
    }

    std::string ebc = ebcCommand();

    /// [lib] and [bin] are independent targets - a package may have either or
    /// both, each invoking ebc separately (a lib build never produces an
    /// executable, so there's no ordering dependency between the two within
    /// a single package - only across packages, once M5c's dependency graph
    /// exists).
    if (manifest.hasLib) {
        /// Printed to stderr, not stdout (matches Cargo's own convention) -
        /// so `ebpm run > output.txt` captures only the program's real
        /// output, never ebpm's own build noise. Flushed explicitly (not
        /// just "\n") so it's guaranteed to appear before the child
        /// process's own output when both share the same terminal/pipe.
        std::cerr << "   Compiling " << manifest.name << " (lib)" << std::endl;
        std::vector<std::string> args = {
            ebc,
            "--lib",
            (pkgDir / manifest.lib.path).string(),
            "-o",
            (targetDir / manifest.name).string(),
        };
        for (const std::string& dir : extraIncludeDirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        for (const std::string& dir : extraLibDirs) {
            args.push_back("-L");
            args.push_back(dir);
        }
        for (const std::string& lib : extraLibNames) {
            args.push_back("-l");
            args.push_back(lib);
        }
        int rc = ebasic::runProcess(args);
        if (rc != 0) return rc;
    }
    if (manifest.hasBin) {
        std::cerr << "   Compiling " << manifest.bin.name << " (bin)" << std::endl;
        std::vector<std::string> args = {
            ebc,
            (pkgDir / manifest.bin.path).string(),
            "-o",
            binaryPath(manifest, packageDir),
        };
        for (const std::string& dir : extraIncludeDirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        for (const std::string& dir : extraLibDirs) {
            args.push_back("-L");
            args.push_back(dir);
        }
        for (const std::string& lib : extraLibNames) {
            args.push_back("-l");
            args.push_back(lib);
        }
        int rc = ebasic::runProcess(args);
        if (rc != 0) return rc;
    }
    if (manifest.hasSharedLib) {
        std::cerr << "   Compiling " << manifest.sharedLib.name << " (shared-lib)" << std::endl;
        std::vector<std::string> args = {
            ebc,
            "--shared-lib",
            (pkgDir / manifest.sharedLib.path).string(),
            "-o",
            (targetDir / manifest.sharedLib.name).string(),
        };
        for (const std::string& dir : extraIncludeDirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        for (const std::string& dir : extraLibDirs) {
            args.push_back("-L");
            args.push_back(dir);
        }
        for (const std::string& lib : extraLibNames) {
            args.push_back("-l");
            args.push_back(lib);
        }
        int rc = ebasic::runProcess(args);
        if (rc != 0) return rc;
    }
    return 0;
}

namespace {

/// Recursively collects every package `pkg` depends on, directly or
/// transitively (deduplicated via `seen`, keyed by canonical dir - so a
/// diamond dependency is only added once). Needed because a dependency's
/// own `Lib "name"` clause only ever appears in *its own* auto-generated
/// interface file - if `pkg` never `#include`s that file directly (only a
/// closer dependency's interface, which itself `#include`s the transitive
/// one), `pkg`'s compiled module has no way to know that library exists at
/// all, so ebpm must name it explicitly rather than relying on the
/// `Lib`-clause auto-derivation `ebc` normally uses.
///
/// Looks each dependency edge up by *name* (`byName`), not by re-deriving
/// its directory from `Dependency::path` - that field is empty for a `git`
/// dependency (its directory only exists after resolveGitDependency ran),
/// so recomputing it here would silently resolve to the wrong thing (an
/// earlier version did exactly this, and for a git dependency the
/// mis-derived "directory" happened to collapse to `pkg.dir` itself,
/// silently treating the package as its own transitive dependency instead
/// of finding the real one at all).
void collectTransitiveDeps(const ResolvedPackage& pkg,
                            const std::unordered_map<std::string, const ResolvedPackage*>& byName,
                            std::unordered_set<std::string>& seen,
                            std::vector<const ResolvedPackage*>& out) {
    for (const Dependency& dep : effectiveDependencies(pkg.manifest)) {
        auto it = byName.find(dep.name);
        if (it == byName.end()) continue; // unreachable: already resolved by resolveDependencyGraph
        if (!seen.insert(it->second->dir).second) continue; // already collected via another path
        out.push_back(it->second);
        collectTransitiveDeps(*it->second, byName, seen, out);
    }
}

} // namespace

int buildPackageWithDeps(const std::string& rootDir, std::string& err) {
    std::vector<ResolvedPackage> order;
    if (!resolveDependencyGraph(rootDir, order, err)) return 1;

    /// package name -> its resolved entry, consulted by
    /// collectTransitiveDeps to look up each dependency edge's target.
    std::unordered_map<std::string, const ResolvedPackage*> byName;
    for (const ResolvedPackage& pkg : order) byName[pkg.name] = &pkg;

    std::vector<std::string> externalLibDirs = externalLibraryDirs();

    for (const ResolvedPackage& pkg : order) {
        std::unordered_set<std::string> seen;
        std::vector<const ResolvedPackage*> transitiveDeps;
        collectTransitiveDeps(pkg, byName, seen, transitiveDeps);

        std::vector<std::string> extraIncludeDirs;
        std::vector<std::string> extraLibDirs = externalLibDirs;
        std::vector<std::string> extraLibNames;
        std::unordered_set<std::string> seenLibNames;
        for (const std::string& lib : extraLibNames) seenLibNames.insert(lib);
        for (const ResolvedPackage* dep : transitiveDeps) {
            std::string depTargetDir = (fs::path(dep->dir) / "target").string();
            extraIncludeDirs.push_back(depTargetDir);
            extraLibDirs.push_back(depTargetDir);
            if (seenLibNames.insert(dep->name).second) extraLibNames.push_back(dep->name);
            /// A dependency's own raw system-library needs (e.g. a GTK4
            /// binding's `Lib "gtk-4"`) never show up in its own name
            /// (`-l<dep->name>` above only links *that dependency's own
            /// archive*) - see appendLibsSidecar's doc comment for why this
            /// needs a separate file at all.
            appendLibsSidecar(depTargetDir, dep->name, extraLibNames, seenLibNames);
        }
        int rc = buildPackage(pkg.manifest, pkg.dir, extraIncludeDirs, extraLibDirs, extraLibNames, err);
        if (rc != 0) return rc;
    }

    /// Written at the root's own *canonical* directory (order's last entry -
    /// see resolveDependencyGraph), not the raw, possibly-relative `rootDir`
    /// argument, so the lockfile always lands next to the manifest that was
    /// actually resolved, regardless of what path the caller happened to
    /// pass in.
    if (!writeLockfile(order.back().dir, order, err)) return 1;
    return 0;
}

bool computeConsumerDirs(const std::string& rootDir, std::vector<std::string>& includeDirs,
                          std::vector<std::string>& libDirs, std::vector<std::string>& libNames,
                          std::string& err) {
    std::vector<ResolvedPackage> order;
    if (!resolveDependencyGraph(rootDir, order, err)) return false;

    std::unordered_map<std::string, const ResolvedPackage*> byName;
    for (const ResolvedPackage& pkg : order) byName[pkg.name] = &pkg;

    /// resolveDependencyGraph's own doc comment guarantees the root is
    /// always last.
    const ResolvedPackage& root = order.back();
    std::unordered_set<std::string> seen;
    std::vector<const ResolvedPackage*> transitiveDeps;
    collectTransitiveDeps(root, byName, seen, transitiveDeps);

    for (const std::string& dir : externalLibraryDirs()) libDirs.push_back(dir);

    std::unordered_set<std::string> seenLibNames;
    for (const ResolvedPackage* dep : transitiveDeps) {
        std::string depTargetDir = (fs::path(dep->dir) / "target").string();
        includeDirs.push_back(depTargetDir);
        libDirs.push_back(depTargetDir);
        if (seenLibNames.insert(dep->name).second) libNames.push_back(dep->name);
        appendLibsSidecar(depTargetDir, dep->name, libNames, seenLibNames);
    }
    /// The root's *own* target dir/name too - unlike buildPackageWithDeps's
    /// per-package loop (which never needs a package to see its own output
    /// while building itself), a *consumer* of the root package (a test
    /// file) needs exactly what an external dependent package would: the
    /// root's interface file to #include and its archive to link.
    if (root.manifest.hasLib) {
        std::string rootTargetDir = (fs::path(root.dir) / "target").string();
        includeDirs.push_back(rootTargetDir);
        libDirs.push_back(rootTargetDir);
        if (seenLibNames.insert(root.name).second) libNames.push_back(root.name);
        appendLibsSidecar(rootTargetDir, root.name, libNames, seenLibNames);
    }
    return true;
}

} // namespace ebpm
