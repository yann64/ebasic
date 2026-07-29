#pragma once

#include "manifest.hpp"

#include <string>
#include <vector>

namespace ebpm {

/// How a `ResolvedPackage` was reached - `Root` for the package
/// `resolveDependencyGraph` was itself called on (not reached via any
/// dependency edge at all), otherwise which kind of dependency edge first
/// reached it (a diamond dependency's `sourceKind`/`version` reflect
/// whichever edge happened to resolve it first - see resolveDependencyGraph's
/// own memoization).
enum class SourceKind { Root, Path, Git, Registry };

/// A package that's part of a resolved dependency graph: its manifest, the
/// canonical absolute directory it was resolved to, how it was reached
/// (`sourceKind`), and (only for `Git`/`Registry`) the exact commit it was
/// checked out at. `version` is only set for `Registry` - the concrete
/// version REG-4's resolver picked to satisfy whichever requirement
/// reached it first.
struct ResolvedPackage {
    std::string name;
    std::string dir; // canonical absolute path
    Manifest manifest;
    std::string gitCommit;
    SourceKind sourceKind = SourceKind::Root;
    std::string version; // only meaningful when sourceKind == Registry
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
/// even if the remote branch has since moved. A `version` (registry)
/// dependency picks the highest version in the package index satisfying
/// its requirement, then resolves exactly like a `git` dependency from
/// there (the index entry's own git/tag) - deliberately non-backtracking:
/// each package *name* resolves to exactly one version for the whole
/// graph, memoized separately from the directory-keyed memoization below,
/// so a second edge naming the same package with a compatible requirement
/// reuses the first pick, while a genuinely incompatible one is a hard,
/// clearly-messaged error rather than an attempt to keep two versions.
///
/// A dependency directory with no [lib] target is rejected (nothing for a
/// dependent package to link against). Detects cycles (a package that
/// transitively depends on itself) rather than recursing forever.
bool resolveDependencyGraph(const std::string& rootDir, std::vector<ResolvedPackage>& order,
                            std::string& err);

} // namespace ebpm
