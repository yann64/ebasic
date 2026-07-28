#pragma once

#include "resolve.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace ebpm {

// Writes `<rootDir>/ebasic.lock`, recording every dependency in `order`
// (everything except `order`'s own last entry, which is always the root
// package itself - see resolveDependencyGraph) as a name -> canonical
// absolute directory pair, plus a `commit` field for anything resolved via
// a `git` dependency edge (`ResolvedPackage::gitCommit`, empty for a
// `path` dependency). Regenerated on every `ebpm build`.
bool writeLockfile(const std::string& rootDir, const std::vector<ResolvedPackage>& order,
                    std::string& err);

// Reads back `<rootDir>/ebasic.lock`'s pinned commits, keyed by dependency
// name - only entries that have a `commit` field are included (a `path`
// dependency never does). Consulted by resolveDependencyGraph before
// resolving a `git` dependency, so a repeat build checks out the exact
// same commit rather than whatever `branch`/`tag`/`rev` currently resolves
// to upstream - this is the whole point of a lockfile for a git
// dependency, and only starts to matter once M5d's git support exists (a
// `path` dependency has nothing worth pinning: it always resolves to the
// same directory, whose own content is whatever's on disk right now, not
// something a commit-style pin could freeze). Returns an empty map (not a
// failure) if the lockfile doesn't exist yet - a first build has nothing
// to read back.
std::unordered_map<std::string, std::string> readLockfilePins(const std::string& rootDir);

} // namespace ebpm
