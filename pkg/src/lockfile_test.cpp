/// Standalone unit test for pkg/src/lockfile.{hpp,cpp} - regression test
/// for a real bug found via Windows CI: a resolved package's `dir` (a
/// canonicalized *native* filesystem path - backslash-separated on
/// Windows, unlike a URL or a Unix-style path) was written into the
/// lockfile with no TOML string escaping at all, silently corrupting the
/// whole file (`\a`/`\_`/`\m`/... aren't valid TOML escape sequences) -
/// every later `readLockfilePins` call then caught the resulting parse
/// error and treated it as "nothing pinned yet", breaking pin-reuse
/// completely and invisibly. Reproduces the failure mode portably (no
/// actual Windows path needed) by constructing a synthetic
/// `ResolvedPackage` whose fields deliberately contain a backslash and a
/// double quote, and confirms every value round-trips exactly through
/// `writeLockfile` -> `readLockfilePins`.
#include "lockfile.hpp"

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

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
    fs::path tmpDir = fs::temp_directory_path() / "ebpm_lockfile_test";
    std::error_code ec;
    fs::create_directories(tmpDir, ec);

    ebpm::ResolvedPackage pkg;
    pkg.name = "trickylib";
    // Deliberately contains a backslash and a double quote - exactly the
    // shape a real Windows canonical path (`C:\Users\...`) or an
    // adversarial value could take; must round-trip exactly.
    pkg.dir = "C:\\Users\\test\\lib\"quoted\"";
    pkg.gitCommit = "abc123";
    pkg.sourceKind = ebpm::SourceKind::Registry;
    pkg.version = "1.0.0";
    pkg.registryGit = "C:\\path\\to\\repo.git";
    pkg.registryRef = "v1.0.0";

    ebpm::ResolvedPackage root;
    root.name = "root";
    root.sourceKind = ebpm::SourceKind::Root;

    std::vector<ebpm::ResolvedPackage> order = {pkg, root};
    std::string err;
    check(ebpm::writeLockfile(tmpDir.string(), order, err), "writeLockfile succeeds");

    auto pins = ebpm::readLockfilePins(tmpDir.string());
    auto it = pins.find("trickylib");
    check(it != pins.end(), "readLockfilePins finds the tricky-path entry at all "
                             "(a real TOML-corruption bug would make this silently empty)");
    if (it != pins.end()) {
        check(it->second.commit == "abc123", "commit round-trips");
        check(it->second.version == "1.0.0", "version round-trips");
        check(it->second.git == "C:\\path\\to\\repo.git",
              "git URL with backslashes round-trips exactly");
        check(it->second.ref == "v1.0.0", "ref round-trips");
    }

    fs::remove_all(tmpDir, ec);

    if (failures == 0) {
        std::printf("PASS: lockfile TOML-escaping round-trip\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
