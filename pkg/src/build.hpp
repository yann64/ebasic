#pragma once

#include "manifest.hpp"
#include "resolve.hpp"

#include <string>
#include <vector>

namespace ebpm {

// Which real `ebc` binary to invoke - the `EBC` environment variable if
// set, else the bare name "ebc" (left for the OS's own PATH search) -
// mirrors ebc's own `CXX`-environment-variable-else-"g++" convention
// exactly (see main.cpp), so a dev build tree (where ebc/ebpm live in
// separate CMake target directories, not side by side) can point ebpm at
// the right binary without installing anything.
std::string ebcCommand();

// Builds `manifest`'s target(s) (rooted at `packageDir`) into
// `<packageDir>/target/`, invoking ebcCommand() once per target ([lib] and
// [bin] are independent - a package may have either, or both). Prints one
// short progress line per target (matches Cargo's own "Compiling ..."
// feel). `extraIncludeDirs`/`extraLibDirs` are forwarded as repeated
// `-I`/`-L` flags, and `extraLibNames` as repeated `-l` flags (needed
// alongside `-L` for a *transitive* dependency, whose own `Lib "name"`
// clause never appears in this module at all - see `ebc`'s own
// `Options::extraLibNames` doc comment) - all three empty until M5c's
// dependency resolution needs them, threaded through now so that slice
// doesn't need to reshape this signature. Returns the first non-zero ebc
// exit code encountered (0 if every target built successfully); `err` is
// set only for a build-orchestration failure that never reaches ebc at
// all.
int buildPackage(const Manifest& manifest, const std::string& packageDir,
                  const std::vector<std::string>& extraIncludeDirs,
                  const std::vector<std::string>& extraLibDirs,
                  const std::vector<std::string>& extraLibNames, std::string& err);

// Resolves the full dependency graph rooted at `rootDir` (resolveDependencyGraph),
// builds every package in it in dependency order - each package receiving
// `-I`/`-L`/`-l` for its *entire transitive* dependency closure, not just its
// direct dependencies (needed so a chain like app -> mid -> base still links
// `base` into `app`'s final binary even though `app`'s own source never
// `#include`s `base`'s interface file directly) - and writes/updates
// `<rootDir>/ebasic.lock`. Returns the first non-zero ebc exit code
// encountered (0 on success); `err` is set for a resolution/orchestration
// failure that never reaches ebc at all (an unresolvable path, a cycle, a
// dependency with no [lib] target, a git dependency - not supported until
// M5d).
int buildPackageWithDeps(const std::string& rootDir, std::string& err);

// Path to the package's built executable (`<packageDir>/target/<bin.name>`)
// - only meaningful when `manifest.hasBin`.
std::string binaryPath(const Manifest& manifest, const std::string& packageDir);

// Path to the package's built static archive
// (`<packageDir>/target/lib<name>.a`) - only meaningful when
// `manifest.hasLib`.
std::string archivePath(const Manifest& manifest, const std::string& packageDir);

// Path to the package's auto-generated interface file
// (`<packageDir>/target/<name>.iface.bas`) - only meaningful when
// `manifest.hasLib`.
std::string interfacePath(const Manifest& manifest, const std::string& packageDir);

// True if `outPath` doesn't exist, or exists but is older than any of
// `srcPaths` - a deliberately simple staleness check (each relevant
// package's single primary source file's mtime - a [bin]'s or [lib]'s
// declared `path` - no #include dependency-chain tracking within a
// package, no content hashing) good enough for `ebpm run` to skip a
// redundant rebuild on a repeat invocation, across the whole resolved
// dependency graph (a dependency's own source changing must also trigger a
// rebuild of whatever depends on it, not just the root's own source).
bool isStale(const std::vector<std::string>& srcPaths, const std::string& outPath);

} // namespace ebpm
