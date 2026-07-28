#include "resolve.hpp"
#include "gitdep.hpp"
#include "lockfile.hpp"

#include <filesystem>
#include <functional>
#include <unordered_map>

namespace fs = std::filesystem;

namespace ebpm {

bool resolveDependencyGraph(const std::string& rootDir, std::vector<ResolvedPackage>& order,
                             std::string& err) {
    std::error_code rootEc;
    fs::path canonicalRoot = fs::canonical(rootDir, rootEc);
    if (rootEc) {
        err = "cannot resolve package directory '" + rootDir + "': " + rootEc.message();
        return false;
    }
    // Read once, up front, from the root's own ebasic.lock - a git
    // dependency anywhere in the graph consults this by name (see
    // resolveGitDependency's `pinnedCommit` parameter) so a repeat build
    // stays reproducible instead of re-resolving whatever `branch` says
    // right now.
    std::unordered_map<std::string, std::string> pins = readLockfilePins(canonicalRoot.string());

    // canonical dir -> its already-resolved manifest (also serves as the
    // "fully resolved" memo, so a diamond dependency is only loaded once).
    std::unordered_map<std::string, Manifest> resolvedByDir;
    // Canonical dirs currently on the DFS stack (an ancestor still being
    // resolved) - a dependency edge landing back on one of these is a cycle.
    std::vector<std::string> visiting;

    std::function<bool(const std::string&, const std::string&)> visit =
        [&](const std::string& dir, const std::string& gitCommit) -> bool {
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
        for (const Dependency& dep : manifest.dependencies) {
            std::string depDir;
            std::string depGitCommit;
            if (!dep.path.empty()) {
                depDir = (canonicalDir / dep.path).string();
            } else {
                auto pinIt = pins.find(dep.name);
                std::string pinnedCommit = pinIt != pins.end() ? pinIt->second : std::string();
                if (!resolveGitDependency(dep, pinnedCommit, depDir, depGitCommit, err)) return false;
            }
            if (!visit(depDir, depGitCommit)) return false;

            std::error_code depEc;
            std::string depKey = fs::canonical(depDir, depEc).string();
            // A resolved dependency must have a [lib] target - there's
            // nothing else for a dependent package to link against (a
            // dependency's own [bin], if any, is irrelevant to whoever
            // depends on it). `depEc` can't be set here (visit() already
            // canonicalized this same path successfully above).
            if (!resolvedByDir[depKey].hasLib) {
                err = "dependency '" + dep.name + "' (at '" + depKey + "') has no [lib] target to "
                      "build against";
                return false;
            }
        }
        visiting.pop_back();

        order.push_back(ResolvedPackage{manifest.name, key, manifest, gitCommit});
        resolvedByDir[key] = std::move(manifest);
        return true;
    };

    return visit(canonicalRoot.string(), "");
}

} // namespace ebpm
