#include "manifest.hpp"
#include "semver.hpp"

#include <algorithm>
#include <filesystem>
#include <toml++/toml.hpp>

namespace ebpm {

std::string currentTargetOS() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__HAIKU__)
    return "haiku";
#else
    return "linux";
#endif
}

std::vector<Dependency> effectiveDependencies(const Manifest& manifest) {
    std::vector<Dependency> result = manifest.dependencies;
    auto it = manifest.targetDependencies.find(currentTargetOS());
    if (it != manifest.targetDependencies.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    return result;
}

namespace {

/// Shared per-entry parsing/validation for one `name = "..."` (a registry
/// version-requirement shorthand) or `name = { path = "..." }` (or
/// `git = "..."`, or `version = "..."`) style dependency entry - used
/// identically for both the top-level `[dependencies]` table and each
/// `[target.<os>.dependencies]` table, so the exactly-one-of-path-git-
/// version and at-most-one-of-branch-tag-rev rules are enforced in exactly
/// one place. `sectionLabel` is only used to make `err` point at the right
/// section (e.g. "[dependencies]" or "[target.linux.dependencies]").
bool parseDependencyEntry(const std::string& sectionLabel, const toml::key& key,
                           const toml::node& val, Dependency& dep, std::string& err) {
    dep.name = std::string(key.str());

    /// `name = "^1.2.0"` shorthand for a registry dependency - equivalent
    /// to `name = { version = "^1.2.0" }` below (Cargo's own
    /// `serde = "1.0"` shorthand has the same shape).
    if (val.is_string()) {
        dep.version = *val.value<std::string>();
        VersionReq req;
        std::string reqErr;
        if (!parseVersionReq(dep.version, req, reqErr)) {
            err = sectionLabel + "." + dep.name + ": " + reqErr;
            return false;
        }
        return true;
    }
    if (!val.is_table()) {
        err = sectionLabel + "." + dep.name +
              " must be a version requirement string (e.g. \"^1.2.0\") or an inline table "
              "(e.g. { path = \"...\" }, { git = \"...\" }, or { version = \"...\" })";
        return false;
    }
    const toml::table& depTbl = *val.as_table();
    dep.path = depTbl["path"].value<std::string>().value_or("");
    dep.git = depTbl["git"].value<std::string>().value_or("");
    dep.version = depTbl["version"].value<std::string>().value_or("");
    dep.branch = depTbl["branch"].value<std::string>().value_or("");
    dep.tag = depTbl["tag"].value<std::string>().value_or("");
    dep.rev = depTbl["rev"].value<std::string>().value_or("");

    int kindCount = (!dep.path.empty()) + (!dep.git.empty()) + (!dep.version.empty());
    if (kindCount != 1) {
        err = sectionLabel + "." + dep.name +
              " must name exactly one of `path`, `git`, or `version`";
        return false;
    }
    int refCount = (!dep.branch.empty()) + (!dep.tag.empty()) + (!dep.rev.empty());
    if (refCount > 1) {
        err = sectionLabel + "." + dep.name +
              " must name at most one of `branch`, `tag`, or `rev`";
        return false;
    }
    if (!dep.version.empty() && refCount > 0) {
        err = sectionLabel + "." + dep.name +
              " may not combine `version` with `branch`/`tag`/`rev` (a registry index entry "
              "already names its own tag)";
        return false;
    }
    if (!dep.version.empty()) {
        VersionReq req;
        std::string reqErr;
        if (!parseVersionReq(dep.version, req, reqErr)) {
            err = sectionLabel + "." + dep.name + ": " + reqErr;
            return false;
        }
    }
    return true;
}

} // namespace

bool loadManifest(const std::string& manifestPath, Manifest& out, std::string& err) {
    if (!std::filesystem::exists(manifestPath)) {
        err = "no such file: '" + manifestPath + "' (not an ebpm package directory?)";
        return false;
    }
    toml::table tbl;
    try {
        tbl = toml::parse_file(manifestPath);
    } catch (const toml::parse_error& e) {
        err = "failed to parse '" + manifestPath + "': " + std::string(e.description());
        return false;
    }

    auto pkg = tbl["package"];
    if (!pkg.is_table()) {
        err = "'" + manifestPath + "' is missing a [package] section";
        return false;
    }
    auto name = pkg["name"].value<std::string>();
    if (!name || name->empty()) {
        err = "'" + manifestPath + "' is missing [package].name";
        return false;
    }
    out.name = *name;
    out.version = pkg["version"].value<std::string>().value_or("0.0.0");

    if (auto lib = tbl["lib"]; lib.is_table()) {
        out.hasLib = true;
        out.lib.path = lib["path"].value<std::string>().value_or("src/lib.bas");
    }
    if (auto bin = tbl["bin"]; bin.is_table()) {
        out.hasBin = true;
        out.bin.name = bin["name"].value<std::string>().value_or(out.name);
        out.bin.path = bin["path"].value<std::string>().value_or("src/main.bas");
    }
    if (!out.hasLib && !out.hasBin) {
        err = "'" + manifestPath + "' declares neither a [lib] nor a [bin] target";
        return false;
    }

    if (auto deps = tbl["dependencies"]; deps.is_table()) {
        for (auto&& [key, val] : *deps.as_table()) {
            Dependency dep;
            if (!parseDependencyEntry("[dependencies]", key, val, dep, err)) return false;
            out.dependencies.push_back(std::move(dep));
        }
    }

    /// `[target.<os>.dependencies]` - one sub-table per platform name, only
    /// ever a fixed, known set (matching CMakePresets.json/CI exactly, see
    /// Manifest::targetDependencies's own doc comment) - an unrecognized
    /// name is almost certainly a typo, so it's rejected rather than
    /// silently ignored (it would otherwise just never build on any
    /// platform, with no clue why).
    if (auto targetTbl = tbl["target"]; targetTbl.is_table()) {
        static const std::vector<std::string> kKnownTargets = {"windows", "linux", "macos",
                                                                 "haiku"};
        for (auto&& [osKey, osVal] : *targetTbl.as_table()) {
            std::string osName(osKey.str());
            if (std::find(kKnownTargets.begin(), kKnownTargets.end(), osName) ==
                kKnownTargets.end()) {
                err = "'[target." + osName +
                      "]' is not a supported target (windows, linux, macos, haiku)";
                return false;
            }
            if (!osVal.is_table()) {
                err = "'[target." + osName + "]' must be a table";
                return false;
            }
            auto osDeps = (*osVal.as_table())["dependencies"];
            if (!osDeps.is_table()) continue; // a target section with no dependencies is a no-op
            std::string sectionLabel = "[target." + osName + ".dependencies]";
            std::vector<Dependency> depsForOs;
            for (auto&& [key, val] : *osDeps.as_table()) {
                Dependency dep;
                if (!parseDependencyEntry(sectionLabel, key, val, dep, err)) return false;
                depsForOs.push_back(std::move(dep));
            }
            out.targetDependencies[osName] = std::move(depsForOs);
        }
    }

    return true;
}

} // namespace ebpm
