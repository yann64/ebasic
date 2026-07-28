#pragma once

#include "resolve.hpp"

#include <string>
#include <vector>

namespace ebpm {

// Writes `<rootDir>/ebasic.lock`, recording every dependency in `order`
// (everything except `order`'s own last entry, which is always the root
// package itself - see resolveDependencyGraph) as a name -> canonical
// absolute directory pair. Regenerated on every `ebpm build`.
//
// For now (M5c, path dependencies only) this is mostly a human-readable
// record of what resolved where, rather than something ebpm reads back to
// change behavior - reading it to *skip* re-resolution is deliberately
// deferred to M5d, where it starts to matter for real (avoiding a network
// fetch to re-check a git branch that hasn't moved); path resolution is
// cheap enough that re-doing it every build costs nothing worth optimizing
// yet. M5d's resolver will also start writing a pinned commit SHA per git
// dependency here, read back on the next build to keep it reproducible
// even if the remote branch has since moved.
bool writeLockfile(const std::string& rootDir, const std::vector<ResolvedPackage>& order,
                    std::string& err);

} // namespace ebpm
