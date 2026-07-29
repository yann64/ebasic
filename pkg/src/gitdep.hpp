#pragma once

#include "manifest.hpp"

#include <string>

namespace ebpm {

/// The current user's home directory (`HOME`, falling back to
/// `USERPROFILE` on Windows, which doesn't always set `HOME` - `"."` if
/// neither is set) - the base every `~/.ebpm/...` cache/config path is
/// computed from (git dependency cache, package index cache, and the
/// registry config file).
std::string homeDir();

/// A filesystem-safe cache-directory name for a URL - every character
/// that isn't alphanumeric/`-`/`_`/`.` becomes `_`. Deliberately readable
/// rather than an opaque hash, so a `~/.ebpm/cache/...` directory stays
/// inspectable by hand. Shared by the git-dependency cache (keyed by URL,
/// see resolveGitDependency) and the package index cache (keyed by index
/// URL, `index.cpp`).
std::string sanitizeForDirName(const std::string& s);

/// Clones `url` into `cacheDir` if it doesn't already look like a git
/// checkout (no `.git` subdirectory yet), else fetches to refresh it -
/// shared by resolveGitDependency (below, for a `git`-typed dependency) and
/// the package index fetch (`index.cpp`, for the registry itself), so
/// there's exactly one place that knows how to keep a cached clone
/// up to date. Prints the same "Cloning .../Fetching ..." progress line
/// either way. Caller is responsible for creating cacheDir's *parent*
/// directory first.
bool cloneOrFetch(const std::string& url, const std::string& cacheDir, std::string& err);

/// Resolves a `git`-typed Dependency into a local, checked-out working
/// directory - cloning into a per-URL subdirectory of a global cache
/// (`~/.ebpm/cache/git/`) if not already present, else fetching to refresh
/// it - and reads back the resulting commit via `git rev-parse HEAD`.
///
/// `pinnedCommit`, if non-empty, is checked out directly (an exact commit
/// SHA previously recorded in `ebasic.lock`) rather than re-resolving
/// `dep.branch`/`dep.tag`/`dep.rev` - this is what makes a repeat build
/// reproducible even if the remote branch has since moved; a fetch still
/// happens first so the pinned commit is retrievable even from a freshly
/// (re-)cloned cache. If `pinnedCommit` is empty (no prior lock entry),
/// `dep.branch`/`tag`/`rev` is checked out instead (preferring a remote
/// tracking ref, e.g. `origin/<branch>`, falling back to the bare ref name
/// for a tag/rev/already-local ref); if none of those are set either, the
/// clone's own default HEAD is used as-is.
///
/// `resolvedDir`/`resolvedCommit` are set on success; `err` on failure (the
/// system `git` binary is missing, the URL can't be cloned/fetched, or the
/// requested ref/pinned commit can't be checked out).
bool resolveGitDependency(const Dependency& dep, const std::string& pinnedCommit,
                          std::string& resolvedDir, std::string& resolvedCommit, std::string& err);

} // namespace ebpm
