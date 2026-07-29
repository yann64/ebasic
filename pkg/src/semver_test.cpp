/// Standalone unit test for pkg/src/semver.{hpp,cpp} - REG-1 of the ebpm
/// registry effort. No existing unit-test-binary precedent exists in this
/// codebase (every other test drives a real compiled tool end-to-end via
/// bash), but SemVer/VersionReq is a pure, dependency-free library with no
/// natural CLI surface of its own yet (that comes in REG-4/REG-6) - a tiny,
/// plain assertion-based binary is the simplest honest way to verify it in
/// isolation before anything else in the plan depends on it.
#include "semver.hpp"

#include <cstdio>
#include <cstdlib>

using ebpm::SemVer;
using ebpm::VersionReq;

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

SemVer sv(int major, int minor, int patch) { return SemVer{major, minor, patch}; }

VersionReq req(const std::string& text) {
    VersionReq r;
    std::string err;
    if (!ebpm::parseVersionReq(text, r, err)) {
        std::fprintf(stderr, "FAIL: parseVersionReq('%s') unexpectedly failed: %s\n", text.c_str(),
                     err.c_str());
        ++failures;
    }
    return r;
}

bool matchesReq(const std::string& reqText, int major, int minor, int patch) {
    return ebpm::matches(req(reqText), sv(major, minor, patch));
}

} // namespace

int main() {
    // --- parseSemVer / toString ---
    {
        SemVer v;
        std::string err;
        check(ebpm::parseSemVer("1.2.3", v, err) && v.major == 1 && v.minor == 2 && v.patch == 3,
              "parseSemVer('1.2.3')");
        check(ebpm::toString(sv(1, 2, 3)) == "1.2.3", "toString(1.2.3)");
        check(!ebpm::parseSemVer("1.2", v, err), "parseSemVer rejects partial 'MAJOR.MINOR'");
        check(!ebpm::parseSemVer("1.2.3.4", v, err), "parseSemVer rejects 4 components");
        check(!ebpm::parseSemVer("01.2.3", v, err), "parseSemVer rejects leading zero");
        check(!ebpm::parseSemVer("1.2.a", v, err), "parseSemVer rejects non-digit component");
        check(!ebpm::parseSemVer("", v, err), "parseSemVer rejects empty string");
    }

    // --- comparison operators ---
    check(sv(1, 2, 3) < sv(1, 2, 4), "1.2.3 < 1.2.4");
    check(sv(1, 2, 3) < sv(1, 3, 0), "1.2.3 < 1.3.0");
    check(sv(1, 2, 3) < sv(2, 0, 0), "1.2.3 < 2.0.0");
    check(sv(1, 2, 3) == sv(1, 2, 3), "1.2.3 == 1.2.3");
    check(sv(2, 0, 0) > sv(1, 9, 9), "2.0.0 > 1.9.9");

    // --- Caret (bare version implies caret) ---
    check(matchesReq("1.2.3", 1, 2, 3), "^1.2.3 (bare) matches 1.2.3");
    check(matchesReq("^1.2.3", 1, 9, 9), "^1.2.3 matches 1.9.9");
    check(!matchesReq("^1.2.3", 2, 0, 0), "^1.2.3 excludes 2.0.0");
    check(!matchesReq("^1.2.3", 1, 2, 2), "^1.2.3 excludes 1.2.2 (below lower bound)");
    check(matchesReq("^1.2", 1, 9, 0), "^1.2 matches 1.9.0");
    check(!matchesReq("^1.2", 2, 0, 0), "^1.2 excludes 2.0.0");
    check(matchesReq("^1", 1, 0, 0), "^1 matches 1.0.0");
    check(!matchesReq("^1", 2, 0, 0), "^1 excludes 2.0.0");

    // --- Caret, 0.x.y edge cases (the tricky part) ---
    check(matchesReq("^0.2.3", 0, 2, 9), "^0.2.3 matches 0.2.9");
    check(!matchesReq("^0.2.3", 0, 3, 0), "^0.2.3 excludes 0.3.0 (minor bump breaks at major=0)");
    check(!matchesReq("^0.2.3", 0, 2, 2), "^0.2.3 excludes 0.2.2");
    check(matchesReq("^0.2", 0, 2, 9), "^0.2 matches 0.2.9");
    check(!matchesReq("^0.2", 0, 3, 0), "^0.2 excludes 0.3.0");
    check(matchesReq("^0.0.3", 0, 0, 3), "^0.0.3 matches only 0.0.3 itself");
    check(!matchesReq("^0.0.3", 0, 0, 4), "^0.0.3 excludes 0.0.4 (patch bump breaks at 0.0.x)");
    check(matchesReq("^0.0", 0, 0, 9), "^0.0 matches 0.0.9");
    check(!matchesReq("^0.0", 0, 1, 0), "^0.0 excludes 0.1.0");
    check(matchesReq("^0", 0, 9, 9), "^0 matches 0.9.9");
    check(!matchesReq("^0", 1, 0, 0), "^0 excludes 1.0.0");
    check(matchesReq("^0.0.0", 0, 0, 0), "^0.0.0 matches exactly 0.0.0");
    check(!matchesReq("^0.0.0", 0, 0, 1), "^0.0.0 excludes 0.0.1");

    // --- Tilde ---
    check(matchesReq("~1.2.3", 1, 2, 9), "~1.2.3 matches 1.2.9");
    check(!matchesReq("~1.2.3", 1, 3, 0), "~1.2.3 excludes 1.3.0");
    check(!matchesReq("~1.2.3", 1, 2, 2), "~1.2.3 excludes 1.2.2");
    check(matchesReq("~1.2", 1, 2, 9), "~1.2 matches 1.2.9");
    check(!matchesReq("~1.2", 1, 3, 0), "~1.2 excludes 1.3.0");
    check(matchesReq("~1", 1, 9, 9), "~1 matches 1.9.9");
    check(!matchesReq("~1", 2, 0, 0), "~1 excludes 2.0.0");

    // --- Exact ---
    check(matchesReq("=1.2.3", 1, 2, 3), "=1.2.3 matches 1.2.3");
    check(!matchesReq("=1.2.3", 1, 2, 4), "=1.2.3 excludes 1.2.4");
    {
        VersionReq r;
        std::string err;
        check(!ebpm::parseVersionReq("=1.2", r, err), "=1.2 (partial exact) is rejected");
        check(!ebpm::parseVersionReq("^1.2.3.4", r, err), "4-component requirement is rejected");
        check(!ebpm::parseVersionReq("^", r, err), "bare '^' with no version is rejected");
        check(!ebpm::parseVersionReq("", r, err), "empty requirement is rejected");
    }

    // --- pickBestSatisfying ---
    {
        std::vector<SemVer> available = {sv(1, 0, 0), sv(1, 1, 0), sv(1, 5, 2), sv(2, 0, 0)};
        auto best = ebpm::pickBestSatisfying(available, req("^1.0.0"));
        check(best.has_value() && *best == sv(1, 5, 2), "pickBestSatisfying picks highest ^1.0.0");
        auto none = ebpm::pickBestSatisfying(available, req("^3.0.0"));
        check(!none.has_value(), "pickBestSatisfying returns nullopt when nothing matches");
    }

    if (failures == 0) {
        std::printf("PASS: all semver checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
