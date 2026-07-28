#include "manifest.hpp"

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

/// Shared per-entry parsing/validation for one `name = { path = "..." }`
/// (or `git = "..."`) style dependency table entry - used identically for
/// both the top-level `[dependencies]` table and each `[target.<os>.
/// dependencies]` table, so the exactly-one-of-path-or-git and
/// at-most-one-of-branch-tag-rev rules are enforced in exactly one place.
/// `sectionLabel` is only used to make `err` point at the right section
/// (e.g. "[dependencies]" or "[target.linux.dependencies]").
bool parseDependencyEntry(const std::string& sectionLabel, const toml::key& key,
                           const toml::node& val, Dependency& dep, std::string& err) {
    if (!val.is_table()) {
        err = sectionLabel + "." + std::string(key.str()) +
              " must be an inline table (e.g. { path = \"...\" } or { git = \"...\" })";
        return false;
    }
    const toml::table& depTbl = *val.as_table();
    dep.name = std::string(key.str());
    dep.path = depTbl["path"].value<std::string>().value_or("");
    dep.git = depTbl["git"].value<std::string>().value_or("");
    dep.branch = depTbl["branch"].value<std::string>().value_or("");
    dep.tag = depTbl["tag"].value<std::string>().value_or("");
    dep.rev = depTbl["rev"].value<std::string>().value_or("");
    // Exactly one of path/git must be set: both empty or both non-empty are
    // equally invalid, hence the equality check rather than an explicit XOR.
    if (dep.path.empty() == dep.git.empty()) {
        err = sectionLabel + "." + dep.name + " must name exactly one of `path` or `git`";
        return false;
    }
    int refCount = (!dep.branch.empty()) + (!dep.tag.empty()) + (!dep.rev.empty());
    if (refCount > 1) {
        err = sectionLabel + "." + dep.name +
              " must name at most one of `branch`, `tag`, or `rev`";
        return false;
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
