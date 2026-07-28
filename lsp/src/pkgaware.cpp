#include "pkgaware.hpp"
#include "symbols.hpp"

#include "build.hpp"
#include "manifest.hpp"
#include "resolve.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ebasic::lsp {

std::optional<std::string> findPackageRoot(const std::string& startDir) {
    std::error_code ec;
    fs::path dir = fs::path(startDir);
    if (!dir.is_absolute()) dir = fs::absolute(dir, ec);
    while (true) {
        if (fs::exists(dir / "ebasic.toml", ec)) {
            fs::path canonical = fs::canonical(dir, ec);
            return (ec ? dir : canonical).string();
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break; // reached the filesystem root
        dir = parent;
    }
    return std::nullopt;
}

namespace {

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

} // namespace

PackageContext resolvePackageContext(const std::string& packageDir) {
    PackageContext ctx;

    std::vector<std::string> libDirs;
    std::vector<std::string> libNames;
    std::string err;
    // A failure here (a cycle, an unresolvable path) just leaves
    // includeDirs empty - the caller degrades to no extra -I dirs.
    ebpm::computeConsumerDirs(packageDir, ctx.includeDirs, libDirs, libNames, err);

    std::vector<ebpm::ResolvedPackage> order;
    if (!ebpm::resolveDependencyGraph(packageDir, order, err) || order.empty()) {
        return ctx;
    }

    // resolveDependencyGraph's own contract: the root package is always
    // last - every earlier entry is a real (direct or transitive)
    // dependency.
    for (size_t i = 0; i + 1 < order.size(); ++i) {
        const ebpm::ResolvedPackage& dep = order[i];
        if (!dep.manifest.hasLib) continue;
        std::string ifacePath = ebpm::interfacePath(dep.manifest, dep.dir);
        std::optional<std::string> content = readFile(ifacePath);
        if (!content) {
            ctx.missingInterfaces.push_back(dep.name); // never built - see PackageContext's doc comment
            continue;
        }
        std::optional<CheckedDocument> checked = checkDocument(ifacePath, *content);
        if (checked) {
            ctx.dependencies.emplace(dep.name, DependencyInterface{ifacePath, std::move(checked->index)});
        }
    }
    return ctx;
}

} // namespace ebasic::lsp
