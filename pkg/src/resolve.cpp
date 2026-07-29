#include "resolve.hpp"
#include "gitdep.hpp"
#include "index.hpp"
#include "lockfile.hpp"
#include "semver.hpp"

#include <filesystem>
#include <functional>
#include <unordered_map>

namespace fs = std::filesystem;

namespace ebpm {

namespace {

/// The one version REG-4's resolver picked for a registry package *name*,
/// the first time it was encountered anywhere in the graph - see
/// resolveDependencyGraph's own doc comment on why this is memoized by
/// name rather than by directory (unlike everything else here).
struct RegistryPick {
    SemVer version;
    std::string dir;
    std::string gitCommit;
    std::string requirement; ///< the requirement string that won this pick
    std::string requirer;    ///< which package name asked for it first
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
    /// Read once, up front, from the root's own ebasic.lock - a git
    /// dependency anywhere in the graph consults this by name (see
    /// resolveGitDependency's `pinnedCommit` parameter) so a repeat build
    /// stays reproducible instead of re-resolving whatever `branch` says
    /// right now.
    std::unordered_map<std::string, std::string> pins = readLockfilePins(canonicalRoot.string());

    /// canonical dir -> its already-resolved manifest (also serves as the
    /// "fully resolved" memo, so a diamond dependency is only loaded once).
    std::unordered_map<std::string, Manifest> resolvedByDir;
    /// Canonical dirs currently on the DFS stack (an ancestor still being
    /// resolved) - a dependency edge landing back on one of these is a cycle.
    std::vector<std::string> visiting;
    /// Package name -> the one version picked for it - see RegistryPick's
    /// own doc comment.
    std::unordered_map<std::string, RegistryPick> resolvedRegistryVersions;

    std::function<bool(const std::string&, const std::string&, SourceKind, const std::string&)> visit =
        [&](const std::string& dir, const std::string& gitCommit, SourceKind sourceKind,
            const std::string& version) -> bool {
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
            SourceKind depSourceKind;
            std::string depVersion;
            if (!dep.path.empty()) {
                depDir = (canonicalDir / dep.path).string();
                depSourceKind = SourceKind::Path;
            } else if (!dep.git.empty()) {
                auto pinIt = pins.find(dep.name);
                std::string pinnedCommit = pinIt != pins.end() ? pinIt->second : std::string();
                if (!resolveGitDependency(dep, pinnedCommit, depDir, depGitCommit, err)) return false;
                depSourceKind = SourceKind::Git;
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
                    depVersion = toString(already->second.version);
                } else {
                    PackageIndex pkgIndex;
                    if (!lookupPackage(dep.name, pkgIndex, err)) return false;
                    VersionReq req;
                    if (!parseVersionReq(dep.version, req, err)) return false;
                    std::vector<SemVer> available;
                    for (const IndexVersionEntry& v : pkgIndex.versions) available.push_back(v.version);
                    auto chosen = pickBestSatisfying(available, req);
                    if (!chosen) {
                        err = "no version of '" + dep.name + "' in the index satisfies " + dep.version;
                        return false;
                    }
                    const IndexVersionEntry* chosenEntry = nullptr;
                    for (const IndexVersionEntry& v : pkgIndex.versions) {
                        if (v.version == *chosen) {
                            chosenEntry = &v;
                            break;
                        }
                    }
                    Dependency synthetic;
                    synthetic.name = dep.name;
                    synthetic.git = chosenEntry->git;
                    synthetic.branch = chosenEntry->branch;
                    synthetic.tag = chosenEntry->tag;
                    synthetic.rev = chosenEntry->rev;
                    auto pinIt = pins.find(dep.name);
                    std::string pinnedCommit = pinIt != pins.end() ? pinIt->second : std::string();
                    if (!resolveGitDependency(synthetic, pinnedCommit, depDir, depGitCommit, err)) {
                        return false;
                    }
                    resolvedRegistryVersions[dep.name] =
                        RegistryPick{*chosen, depDir, depGitCommit, dep.version, manifest.name};
                    depVersion = toString(*chosen);
                }
                depSourceKind = SourceKind::Registry;
            }
            if (!visit(depDir, depGitCommit, depSourceKind, depVersion)) return false;

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

        order.push_back(ResolvedPackage{manifest.name, key, manifest, gitCommit, sourceKind, version});
        resolvedByDir[key] = std::move(manifest);
        return true;
    };

    return visit(canonicalRoot.string(), "", SourceKind::Root, "");
}

} // namespace ebpm
