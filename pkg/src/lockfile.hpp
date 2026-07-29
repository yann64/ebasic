#pragma once

#include "resolve.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace ebpm {

/// Writes `<rootDir>/ebasic.lock`, recording every dependency in `order`
/// (everything except `order`'s own last entry, which is always the root
/// package itself - see resolveDependencyGraph) as a name -> canonical
/// absolute directory pair, plus a `commit` field for anything resolved via
/// a `git` dependency edge (`ResolvedPackage::gitCommit`, empty for a
/// `path` dependency). A `Registry`-sourced entry additionally records
/// `version`, `git`, and (if the chosen index entry named one) `ref` -
/// REG-5: a registry dependency has no URL in the manifest at all (only a
/// name + requirement string), unlike a plain `git` dependency whose URL
/// always lives in the manifest itself, so a pinned rebuild needs the
/// lockfile to carry the resolved URL/ref directly or it would have no way
/// to avoid consulting the index again. Regenerated on every `ebpm build`.
bool writeLockfile(const std::string& rootDir, const std::vector<ResolvedPackage>& order,
                    std::string& err);

/// One `[[package]]` entry's pinned data, keyed by dependency name (see
/// readLockfilePins below). `commit` is set for anything resolved via a
/// `git` or `Registry` dependency edge; `version`/`git`/`ref` are only set
/// for a `Registry` entry - `ref` is whichever of `branch`/`tag`/`rev` the
/// originally-chosen index entry named (reconstructed generically here,
/// since which specific keyword it was doesn't matter on a pinned rebuild -
/// `resolveGitDependency`'s pinned-commit path always wins over
/// branch/tag/rev regardless; `ref`'s only remaining purpose is keeping
/// gitdep.cpp's cache-directory key - see its REG-0 fix - stable across
/// resolutions of the same edge).
struct Pin {
    std::string commit;
    std::string version;
    std::string git;
    std::string ref;
};

/// Reads back `<rootDir>/ebasic.lock`'s pins, keyed by dependency name.
/// Consulted by resolveDependencyGraph before resolving a `git` or
/// `Registry` dependency, so a repeat build checks out the exact same
/// commit rather than whatever `branch`/`tag`/`rev` currently resolves to
/// upstream (and, for a `Registry` dependency whose pinned version still
/// satisfies its current requirement, without consulting the index at all)
/// - this is the whole point of a lockfile for a non-`path` dependency (a
/// `path` dependency has nothing worth pinning: it always resolves to the
/// same directory, whose own content is whatever's on disk right now, not
/// something a commit-style pin could freeze). Returns an empty map (not a
/// failure) if the lockfile doesn't exist yet - a first build has nothing
/// to read back.
std::unordered_map<std::string, Pin> readLockfilePins(const std::string& rootDir);

} // namespace ebpm
