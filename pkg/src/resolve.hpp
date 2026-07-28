#pragma once

#include "manifest.hpp"

#include <string>
#include <vector>

namespace ebpm {

/// A package that's part of a resolved dependency graph: its manifest, the
/// canonical absolute directory it was resolved to, and (only for a package
/// reached via a `git` dependency edge) the exact commit it was checked out
/// at - empty for the root package and for anything reached via a `path`
/// dependency.
struct ResolvedPackage {
    std::string name;
    std::string dir; // canonical absolute path
    Manifest manifest;
    std::string gitCommit;
};

/// Resolves the full transitive dependency graph rooted at `rootDir`,
/// returning every package in it - including the root itself, always last -
/// in dependency-first order: a package's own dependencies always appear
/// before it, so building `order` front-to-back satisfies every ordering
/// requirement with no further sorting needed. A package reachable through
/// more than one path (a diamond dependency) appears exactly once, at its
/// first-resolved position.
///
/// A `path` dependency resolves directly to that directory. A `git`
/// dependency is cloned/fetched into a global cache (see gitdep.hpp) - if
/// `<rootDir>/ebasic.lock` already pins a commit for it (from a prior
/// build), that exact commit is checked out again rather than
/// re-resolving `branch`/`tag`/`rev`, so a repeat build stays reproducible
/// even if the remote branch has since moved.
///
/// A dependency directory with no [lib] target is rejected (nothing for a
/// dependent package to link against). Detects cycles (a package that
/// transitively depends on itself) rather than recursing forever.
bool resolveDependencyGraph(const std::string& rootDir, std::vector<ResolvedPackage>& order,
                            std::string& err);

} // namespace ebpm
