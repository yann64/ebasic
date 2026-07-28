#pragma once

#include "manifest.hpp"

#include <string>
#include <vector>

namespace ebpm {

// A package that's part of a resolved dependency graph: its manifest, plus
// the canonical absolute directory it was resolved to.
struct ResolvedPackage {
    std::string name;
    std::string dir; // canonical absolute path
    Manifest manifest;
};

// Resolves the full transitive dependency graph rooted at `rootDir`,
// returning every package in it - including the root itself, always last -
// in dependency-first order: a package's own dependencies always appear
// before it, so building `order` front-to-back satisfies every ordering
// requirement with no further sorting needed. A package reachable through
// more than one path (a diamond dependency) appears exactly once, at its
// first-resolved position.
//
// Only `path` dependencies are resolved for now - a `git` dependency is
// rejected with a clear "not yet supported" message (M5d) rather than
// silently skipped. A dependency directory with no [lib] target is
// rejected too (nothing for a dependent package to link against). Detects
// cycles (a package that transitively depends on itself) rather than
// recursing forever.
bool resolveDependencyGraph(const std::string& rootDir, std::vector<ResolvedPackage>& order,
                            std::string& err);

} // namespace ebpm
