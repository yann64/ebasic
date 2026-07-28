#include "manifest.hpp"

#include <filesystem>
#include <toml++/toml.hpp>

namespace ebpm {

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
            if (!val.is_table()) {
                err = "[dependencies]." + std::string(key.str()) + " must be an inline table (e.g. "
                      "{ path = \"...\" } or { git = \"...\" })";
                return false;
            }
            const toml::table& depTbl = *val.as_table();
            Dependency dep;
            dep.name = std::string(key.str());
            dep.path = depTbl["path"].value<std::string>().value_or("");
            dep.git = depTbl["git"].value<std::string>().value_or("");
            dep.branch = depTbl["branch"].value<std::string>().value_or("");
            dep.tag = depTbl["tag"].value<std::string>().value_or("");
            dep.rev = depTbl["rev"].value<std::string>().value_or("");
            if (dep.path.empty() == dep.git.empty()) {
                err = "[dependencies]." + dep.name + " must name exactly one of `path` or `git`";
                return false;
            }
            int refCount = (!dep.branch.empty()) + (!dep.tag.empty()) + (!dep.rev.empty());
            if (refCount > 1) {
                err = "[dependencies]." + dep.name + " must name at most one of `branch`, `tag`, "
                      "or `rev`";
                return false;
            }
            out.dependencies.push_back(std::move(dep));
        }
    }

    return true;
}

} // namespace ebpm
