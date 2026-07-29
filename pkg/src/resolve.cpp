#include "resolve.hpp"
#include "gitdep.hpp"
#include "index.hpp"
#include "lockfile.hpp"
#include "semver.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace ebpm {

namespace {

/// The one version REG-4's resolver picked for a registry package *name*,
/// the first time it was encountered anywhere in the graph - see
/// resolveDependencyGraph's own doc comment on why this is memoized by
/// name rather than by directory (unlike everything else here). `git`/
/// `ref` are carried alongside so REG-5's lockfile writer can record them
/// without needing to look anything back up.
struct RegistryPick {
    SemVer version;
    std::string dir;
    std::string gitCommit;
    std::string git;
    std::string ref;
    std::string requirement; ///< the requirement string that won this pick
    std::string requirer;    ///< which package name asked for it first
};

/// Everything about *how* a dependency edge was resolved, threaded into
/// `visit` alongside `dir`/`gitCommit` - bundled into one struct rather
/// than four more positional parameters once REG-5 added the last two.
struct EdgeInfo {
    SourceKind sourceKind = SourceKind::Root;
    std::string version;     // only meaningful for SourceKind::Registry
    std::string registryGit; // only meaningful for SourceKind::Registry
    std::string registryRef; // only meaningful for SourceKind::Registry
};

} // namespace

bool resolveDependencyGraph(const std::string& rootDir, std::vector<ResolvedPackage>& order,
                             std::string& err) {
    std::error_code rootEc;
    fs::path canonicalRoot = fs::canonical(rootDir, rootEc);
    if (rootEc) {
        err = "cannot resolve package directory '" + rootDir + "': " + rootEc.message();
        return false;
    }
    /// Read once, up front, from the root's own ebasic.lock - a git or
    /// registry dependency anywhere in the graph consults this by name (see
    /// resolveGitDependency's `pinnedCommit` parameter, and the registry
    /// branch below) so a repeat build stays reproducible instead of
    /// re-resolving whatever `branch`/the index currently says.
    std::unordered_map<std::string, Pin> pins = readLockfilePins(canonicalRoot.string());

    /// canonical dir -> its already-resolved manifest (also serves as the
    /// "fully resolved" memo, so a diamond dependency is only loaded once).
    std::unordered_map<std::string, Manifest> resolvedByDir;
    /// Canonical dirs currently on the DFS stack (an ancestor still being
    /// resolved) - a dependency edge landing back on one of these is a cycle.
    std::vector<std::string> visiting;
    /// Package name -> the one version picked for it - see RegistryPick's
    /// own doc comment.
    std::unordered_map<std::string, RegistryPick> resolvedRegistryVersions;

    std::function<bool(const std::string&, const std::string&, const EdgeInfo&)> visit =
        [&](const std::string& dir, const std::string& gitCommit, const EdgeInfo& edge) -> bool {
        std::error_code ec;
        fs::path canonicalDir = fs::canonical(dir, ec);
        if (ec) {
            err = "cannot resolve package directory '" + dir + "': " + ec.message();
            return false;
        }
        std::string key = canonicalDir.string();
        if (resolvedByDir.count(key)) return true; // already resolved via another path

        for (const std::string& v : visiting) {
            if (v == key) {
                err = "circular dependency detected involving '" + key + "'";
                return false;
            }
        }

        Manifest manifest;
        std::string manifestPath = (canonicalDir / "ebasic.toml").string();
        if (!loadManifest(manifestPath, manifest, err)) return false;

        visiting.push_back(key);
        for (const Dependency& dep : effectiveDependencies(manifest)) {
            std::string depDir;
            std::string depGitCommit;
            EdgeInfo depEdge;
            if (!dep.path.empty()) {
                depDir = (canonicalDir / dep.path).string();
                depEdge.sourceKind = SourceKind::Path;
            } else if (!dep.git.empty()) {
                auto pinIt = pins.find(dep.name);
                std::string pinnedCommit = pinIt != pins.end() ? pinIt->second.commit : std::string();
                if (!resolveGitDependency(dep, pinnedCommit, depDir, depGitCommit, err)) return false;
                depEdge.sourceKind = SourceKind::Git;
            } else {
                /// A registry (`version`) dependency: pick a concrete
                /// version once per package *name*, not once per edge -
                /// see RegistryPick's own doc comment for why this is a
                /// separate, name-keyed memo rather than reusing
                /// `resolvedByDir`.
                auto already = resolvedRegistryVersions.find(dep.name);
                if (already != resolvedRegistryVersions.end()) {
                    VersionReq req;
                    if (!parseVersionReq(dep.version, req, err)) return false;
                    if (!matches(req, already->second.version)) {
                        err = "no version of '" + dep.name + "' satisfies both " +
                              already->second.requirement + " (required by " +
                              already->second.requirer + ") and " + dep.version + " (required by " +
                              manifest.name + ")";
                        return false;
                    }
                    depDir = already->second.dir;
                    depGitCommit = already->second.gitCommit;
                    depEdge.version = toString(already->second.version);
                    depEdge.registryGit = already->second.git;
                    depEdge.registryRef = already->second.ref;
                } else {
                    /// REG-5: a pinned rebuild whose pinned version still
                    /// satisfies the *current* requirement skips the index
                    /// entirely - reconstructs the exact same synthetic
                    /// git dependency straight from the lockfile, exactly
                    /// like a plain `git` dependency's own pinned rebuild
                    /// never re-resolves `branch`/`tag`/`rev` live. If the
                    /// manifest has since been edited to a requirement the
                    /// pin no longer satisfies, falls through to a fresh
                    /// index lookup below instead (a manifest edit must
                    /// always take effect, not silently stick to a stale
                    /// pin).
                    auto pinIt = pins.find(dep.name);
                    SemVer chosen;
                    std::string chosenGit, chosenRef, chosenCommit;
                    bool haveChoice = false;
                    std::cerr << "TEMPDEBUG pins.size()=" << pins.size() << " found="
                              << (pinIt != pins.end()) << " commit=["
                              << (pinIt != pins.end() ? pinIt->second.commit : "?") << "] version=["
                              << (pinIt != pins.end() ? pinIt->second.version : "?") << "] git=["
                              << (pinIt != pins.end() ? pinIt->second.git : "?") << "] ref=["
                              << (pinIt != pins.end() ? pinIt->second.ref : "?") << "]\n";
                    if (pinIt != pins.end() && !pinIt->second.git.empty() &&
                        !pinIt->second.version.empty()) {
                        SemVer pinnedVersion;
                        std::string pinParseErr;
                        VersionReq req;
                        std::string reqParseErr;
                        if (parseSemVer(pinIt->second.version, pinnedVersion, pinParseErr) &&
                            parseVersionReq(dep.version, req, reqParseErr) && matches(req, pinnedVersion)) {
                            chosen = pinnedVersion;
                            chosenGit = pinIt->second.git;
                            chosenRef = pinIt->second.ref;
                            chosenCommit = pinIt->second.commit;
                            haveChoice = true;
                        }
                    }
                    if (!haveChoice) {
                        PackageIndex pkgIndex;
                        if (!lookupPackage(dep.name, pkgIndex, err)) return false;
                        VersionReq req;
                        if (!parseVersionReq(dep.version, req, err)) return false;
                        std::vector<SemVer> available;
                        for (const IndexVersionEntry& v : pkgIndex.versions) {
                            available.push_back(v.version);
                        }
                        auto pick = pickBestSatisfying(available, req);
                        if (!pick) {
                            err = "no version of '" + dep.name + "' in the index satisfies " +
                                  dep.version;
                            return false;
                        }
                        const IndexVersionEntry* chosenEntry = nullptr;
                        for (const IndexVersionEntry& v : pkgIndex.versions) {
                            if (v.version == *pick) {
                                chosenEntry = &v;
                                break;
                            }
                        }
                        chosen = *pick;
                        chosenGit = chosenEntry->git;
                        chosenRef = !chosenEntry->branch.empty()
                                        ? chosenEntry->branch
                                        : (!chosenEntry->tag.empty() ? chosenEntry->tag : chosenEntry->rev);
                    }
                    Dependency synthetic;
                    synthetic.name = dep.name;
                    synthetic.git = chosenGit;
                    synthetic.tag = chosenRef;
                    if (!resolveGitDependency(synthetic, chosenCommit, depDir, depGitCommit, err)) {
                        return false;
                    }
                    resolvedRegistryVersions[dep.name] = RegistryPick{
                        chosen, depDir, depGitCommit, chosenGit, chosenRef, dep.version, manifest.name};
                    depEdge.version = toString(chosen);
                    depEdge.registryGit = chosenGit;
                    depEdge.registryRef = chosenRef;
                }
                depEdge.sourceKind = SourceKind::Registry;
            }
            if (!visit(depDir, depGitCommit, depEdge)) return false;

            std::error_code depEc;
            std::string depKey = fs::canonical(depDir, depEc).string();
            /// A resolved dependency must have a [lib] target - there's
            /// nothing else for a dependent package to link against (a
            /// dependency's own [bin], if any, is irrelevant to whoever
            /// depends on it). `depEc` can't be set here (visit() already
            /// canonicalized this same path successfully above).
            if (!resolvedByDir[depKey].hasLib) {
                err = "dependency '" + dep.name + "' (at '" + depKey + "') has no [lib] target to "
                      "build against";
                return false;
            }
        }
        visiting.pop_back();

        order.push_back(ResolvedPackage{manifest.name, key, manifest, gitCommit, edge.sourceKind,
                                         edge.version, edge.registryGit, edge.registryRef});
        resolvedByDir[key] = std::move(manifest);
        return true;
    };

    return visit(canonicalRoot.string(), "", EdgeInfo{});
}

} // namespace ebpm
