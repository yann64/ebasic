#pragma once

#include "semver.hpp"

#include <string>
#include <vector>

namespace ebpm {

/// One published version of a package in the index - a name/requirement
/// resolves to this (REG-4's resolver), then to a real git source exactly
/// like a hand-written git dependency (same shape: a git URL plus at most
/// one of `branch`/`tag`/`rev`).
struct IndexVersionEntry {
    SemVer version;
    std::string git;
    std::string branch;
    std::string tag;
    std::string rev;
};

/// One package's full index entry (`<indexDir>/<name>.toml`) - every
/// published version, newest and oldest alike; REG-4's resolver picks
/// whichever one actually satisfies a given requirement.
struct PackageIndex {
    std::string name;
    std::string description;
    std::vector<IndexVersionEntry> versions;
};

/// The index git repository's URL, resolved in priority order: the
/// `EBASIC_INDEX_URL` environment variable, then `~/.ebpm/config.toml`'s
/// `[registry] index = "..."`, then a hardcoded default (this project's own
/// starter index - see docs/architecture/roadmap.md's REG-8 notes).
std::string indexUrl();

/// Clones/fetches the index repository (indexUrl()) into its cache
/// directory (`~/.ebpm/cache/index/<sanitized-index-url>/`) and returns
/// that directory in `indexDir` - shared by lookupPackage and
/// listAllPackages below so a caller doing several lookups only ever
/// fetches once.
bool fetchIndexDir(std::string& indexDir, std::string& err);

/// Looks up `name` in the (fetched/cached) index - false with a clear
/// "no package named ..." message if `<indexDir>/<name>.toml` doesn't
/// exist, not just a raw file-not-found.
bool lookupPackage(const std::string& name, PackageIndex& out, std::string& err);

/// Every package in the (fetched/cached) index - enumerates `*.toml` files
/// directly (no separate "index of the index" catalog - ample at this
/// ecosystem's realistic scale; revisit only if it ever actually becomes a
/// problem, matching this plan's other stated scope cuts). A malformed
/// entry is skipped rather than failing the whole listing - `search`
/// should stay resilient to one bad file.
bool listAllPackages(std::vector<PackageIndex>& out, std::string& err);

} // namespace ebpm
