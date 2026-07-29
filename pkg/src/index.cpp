#include "index.hpp"
#include "gitdep.hpp"

#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

namespace ebpm {

namespace {

fs::path indexCacheRoot() { return fs::path(homeDir()) / ".ebpm" / "cache" / "index"; }

/// This project's own starter index repo (see docs/architecture/
/// roadmap.md's REG-8 notes) - the hardcoded fallback when neither
/// `EBASIC_INDEX_URL` nor `~/.ebpm/config.toml`'s `[registry].index`
/// overrides it.
const char* kDefaultIndexUrl = "https://github.com/yann64/ebpm-index.git";

/// Reads `~/.ebpm/config.toml`'s `[registry] index = "..."`, if present -
/// a missing file, missing section, or missing key all just mean "no
/// override here", never an error (this file is entirely optional).
std::string configuredIndexUrl() {
    fs::path configPath = fs::path(homeDir()) / ".ebpm" / "config.toml";
    std::error_code ec;
    if (!fs::exists(configPath, ec)) return "";
    toml::table tbl;
    try {
        tbl = toml::parse_file(configPath.string());
    } catch (const toml::parse_error&) {
        return "";
    }
    auto registry = tbl["registry"];
    if (!registry.is_table()) return "";
    return (*registry.as_table())["index"].value<std::string>().value_or("");
}

bool parseVersionEntry(const toml::table& tbl, IndexVersionEntry& out, std::string& err) {
    auto versionStr = tbl["version"].value<std::string>();
    if (!versionStr) {
        err = "a [[versions]] entry is missing its own `version` field";
        return false;
    }
    if (!parseSemVer(*versionStr, out.version, err)) return false;
    out.git = tbl["git"].value<std::string>().value_or("");
    out.branch = tbl["branch"].value<std::string>().value_or("");
    out.tag = tbl["tag"].value<std::string>().value_or("");
    out.rev = tbl["rev"].value<std::string>().value_or("");
    if (out.git.empty()) {
        err = "version '" + *versionStr + "' is missing its own `git` URL";
        return false;
    }
    int refCount = (!out.branch.empty()) + (!out.tag.empty()) + (!out.rev.empty());
    if (refCount > 1) {
        err = "version '" + *versionStr + "' must name at most one of `branch`, `tag`, or `rev`";
        return false;
    }
    return true;
}

bool parsePackageIndexFile(const fs::path& path, PackageIndex& out, std::string& err) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path.string());
    } catch (const toml::parse_error& e) {
        err = "failed to parse '" + path.string() + "': " + std::string(e.description());
        return false;
    }
    auto pkg = tbl["package"];
    if (!pkg.is_table()) {
        err = "'" + path.string() + "' is missing a [package] section";
        return false;
    }
    const toml::table& pkgTbl = *pkg.as_table();
    auto name = pkgTbl["name"].value<std::string>();
    if (!name || name->empty()) {
        err = "'" + path.string() + "' is missing [package].name";
        return false;
    }
    out.name = *name;
    out.description = pkgTbl["description"].value<std::string>().value_or("");

    auto versions = tbl["versions"];
    if (!versions.is_array()) {
        err = "'" + path.string() + "' has no [[versions]] entries";
        return false;
    }
    for (auto&& node : *versions.as_array()) {
        auto versionTbl = node.as_table();
        if (!versionTbl) {
            err = "'" + path.string() + "' has a [[versions]] entry that isn't a table";
            return false;
        }
        IndexVersionEntry entry;
        if (!parseVersionEntry(*versionTbl, entry, err)) return false;
        out.versions.push_back(std::move(entry));
    }
    return true;
}

} // namespace

std::string indexUrl() {
    if (const char* env = std::getenv("EBASIC_INDEX_URL")) return env;
    std::string configured = configuredIndexUrl();
    if (!configured.empty()) return configured;
    return kDefaultIndexUrl;
}

bool fetchIndexDir(std::string& indexDir, std::string& err) {
    std::string url = indexUrl();
    fs::path cacheDir = indexCacheRoot() / sanitizeForDirName(url);
    std::error_code ec;
    fs::create_directories(indexCacheRoot(), ec);
    if (!cloneOrFetch(url, cacheDir.string(), err)) return false;
    indexDir = cacheDir.string();
    return true;
}

bool lookupPackage(const std::string& name, PackageIndex& out, std::string& err) {
    std::string indexDir;
    if (!fetchIndexDir(indexDir, err)) return false;
    fs::path entryPath = fs::path(indexDir) / (name + ".toml");
    std::error_code ec;
    if (!fs::exists(entryPath, ec)) {
        err = "no package named '" + name + "' in the index (" + indexUrl() + ")";
        return false;
    }
    return parsePackageIndexFile(entryPath, out, err);
}

bool listAllPackages(std::vector<PackageIndex>& out, std::string& err) {
    std::string indexDir;
    if (!fetchIndexDir(indexDir, err)) return false;
    for (const auto& entry : fs::directory_iterator(indexDir)) {
        if (entry.path().extension() != ".toml") continue;
        PackageIndex pkg;
        std::string parseErr;
        if (!parsePackageIndexFile(entry.path(), pkg, parseErr)) continue; // skip a bad entry
        out.push_back(std::move(pkg));
    }
    return true;
}

} // namespace ebpm
