/// Standalone unit test for pkg/src/index.{hpp,cpp} - REG-3 of the ebpm
/// registry effort. Like semver_test.cpp, index.cpp has no CLI surface of
/// its own yet (that's REG-6/REG-7), so this drives its real functions
/// directly - the harness script (tests/e2e_pkg/run_index_case.sh) points
/// EBASIC_INDEX_URL at a real local bare git repo it seeds with a
/// known-good and a deliberately malformed package entry, and isolates
/// HOME so ~/.ebpm/cache/index/ never touches the real cache.
#include "index.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

} // namespace

int main() {
    ebpm::PackageIndex pkg;
    std::string err;

    check(ebpm::lookupPackage("goodpkg", pkg, err), "lookupPackage('goodpkg') should succeed");
    check(pkg.name == "goodpkg", "goodpkg's own name round-trips");
    check(pkg.description == "a good package", "goodpkg's own description round-trips");
    check(pkg.versions.size() == 2, "goodpkg has 2 published versions");
    if (pkg.versions.size() == 2) {
        check(ebpm::toString(pkg.versions[0].version) == "1.0.0", "goodpkg version[0] is 1.0.0");
        check(pkg.versions[0].tag == "v1.0.0", "goodpkg version[0]'s tag round-trips");
        check(ebpm::toString(pkg.versions[1].version) == "1.1.0", "goodpkg version[1] is 1.1.0");
        check(pkg.versions[1].tag == "v1.1.0", "goodpkg version[1]'s tag round-trips");
        check(!pkg.versions[0].git.empty(), "goodpkg version[0] has a git URL");
    }

    {
        ebpm::PackageIndex missing;
        std::string missingErr;
        check(!ebpm::lookupPackage("nosuchpkg", missing, missingErr),
              "lookupPackage('nosuchpkg') should fail");
        check(missingErr.find("no package named") != std::string::npos,
              "lookupPackage('nosuchpkg')'s error names the package");
    }

    {
        ebpm::PackageIndex bad;
        std::string badErr;
        check(!ebpm::lookupPackage("badpkg", bad, badErr),
              "lookupPackage('badpkg') should fail (missing git URL)");
    }

    {
        std::vector<ebpm::PackageIndex> all;
        std::string listErr;
        check(ebpm::listAllPackages(all, listErr), "listAllPackages should succeed");
        bool foundGood = false, foundBad = false;
        for (const auto& p : all) {
            if (p.name == "goodpkg") foundGood = true;
            if (p.name == "badpkg") foundBad = true;
        }
        check(foundGood, "listAllPackages includes goodpkg");
        check(!foundBad, "listAllPackages silently skips the malformed badpkg entry");
    }

    if (failures == 0) {
        std::printf("PASS: all index checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
