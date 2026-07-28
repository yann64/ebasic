#pragma once

#include "manifest.hpp"

#include <string>

namespace ebpm {

// Resolves a `git`-typed Dependency into a local, checked-out working
// directory - cloning into a per-URL subdirectory of a global cache
// (`~/.ebpm/cache/git/`) if not already present, else fetching to refresh
// it - and reads back the resulting commit via `git rev-parse HEAD`.
//
// `pinnedCommit`, if non-empty, is checked out directly (an exact commit
// SHA previously recorded in `ebasic.lock`) rather than re-resolving
// `dep.branch`/`dep.tag`/`dep.rev` - this is what makes a repeat build
// reproducible even if the remote branch has since moved; a fetch still
// happens first so the pinned commit is retrievable even from a freshly
// (re-)cloned cache. If `pinnedCommit` is empty (no prior lock entry),
// `dep.branch`/`tag`/`rev` is checked out instead (preferring a remote
// tracking ref, e.g. `origin/<branch>`, falling back to the bare ref name
// for a tag/rev/already-local ref); if none of those are set either, the
// clone's own default HEAD is used as-is.
//
// `resolvedDir`/`resolvedCommit` are set on success; `err` on failure (the
// system `git` binary is missing, the URL can't be cloned/fetched, or the
// requested ref/pinned commit can't be checked out).
bool resolveGitDependency(const Dependency& dep, const std::string& pinnedCommit,
                          std::string& resolvedDir, std::string& resolvedCommit, std::string& err);

} // namespace ebpm
