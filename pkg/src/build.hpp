#pragma once

#include "manifest.hpp"

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
// `-I`/`-L` flags - unused (empty) until M5c's dependency resolution needs
// them, threaded through now so that slice doesn't need to reshape this
// signature. Returns the first non-zero ebc exit code encountered (0 if
// every target built successfully); `err` is set only for a build-
// orchestration failure that never reaches ebc at all.
int buildPackage(const Manifest& manifest, const std::string& packageDir,
                  const std::vector<std::string>& extraIncludeDirs,
                  const std::vector<std::string>& extraLibDirs, std::string& err);

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

// True if `outPath` doesn't exist, or exists but is older than `srcPath` -
// a deliberately simple staleness check (single source file's mtime, no
// #include dependency-chain tracking, no content hashing) good enough for
// `ebpm run` to skip a redundant rebuild on a repeat invocation.
bool isStale(const std::string& srcPath, const std::string& outPath);

} // namespace ebpm
