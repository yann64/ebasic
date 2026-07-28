#include "build.hpp"
#include "driver/process.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace ebpm {

std::string ebcCommand() {
    const char* envEbc = std::getenv("EBC");
    return envEbc ? envEbc : "ebc";
}

std::string binaryPath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / manifest.bin.name).string();
}

std::string archivePath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / ("lib" + manifest.name + ".a")).string();
}

std::string interfacePath(const Manifest& manifest, const std::string& packageDir) {
    return (fs::path(packageDir) / "target" / (manifest.name + ".iface.bas")).string();
}

bool isStale(const std::string& srcPath, const std::string& outPath) {
    std::error_code ec;
    if (!fs::exists(outPath, ec)) return true;
    auto outTime = fs::last_write_time(outPath, ec);
    if (ec) return true;
    auto srcTime = fs::last_write_time(srcPath, ec);
    if (ec) return true; // a missing source is a different problem, surfaced by ebc itself
    return srcTime > outTime;
}

int buildPackage(const Manifest& manifest, const std::string& packageDir,
                  const std::vector<std::string>& extraIncludeDirs,
                  const std::vector<std::string>& extraLibDirs, std::string& err) {
    fs::path pkgDir(packageDir);
    fs::path targetDir = pkgDir / "target";
    std::error_code ec;
    fs::create_directories(targetDir, ec);
    if (ec) {
        err = "could not create '" + targetDir.string() + "': " + ec.message();
        return 1;
    }

    std::string ebc = ebcCommand();

    // [lib] and [bin] are independent targets - a package may have either or
    // both, each invoking ebc separately (a lib build never produces an
    // executable, so there's no ordering dependency between the two within
    // a single package - only across packages, once M5c's dependency graph
    // exists).
    if (manifest.hasLib) {
        // Printed to stderr, not stdout (matches Cargo's own convention) -
        // so `ebpm run > output.txt` captures only the program's real
        // output, never ebpm's own build noise. Flushed explicitly (not
        // just "\n") so it's guaranteed to appear before the child
        // process's own output when both share the same terminal/pipe.
        std::cerr << "   Compiling " << manifest.name << " (lib)" << std::endl;
        std::vector<std::string> args = {
            ebc,
            "--lib",
            (pkgDir / manifest.lib.path).string(),
            "-o",
            (targetDir / manifest.name).string(),
        };
        for (const std::string& dir : extraIncludeDirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        for (const std::string& dir : extraLibDirs) {
            args.push_back("-L");
            args.push_back(dir);
        }
        int rc = ebasic::runProcess(args);
        if (rc != 0) return rc;
    }
    if (manifest.hasBin) {
        std::cerr << "   Compiling " << manifest.bin.name << " (bin)" << std::endl;
        std::vector<std::string> args = {
            ebc,
            (pkgDir / manifest.bin.path).string(),
            "-o",
            binaryPath(manifest, packageDir),
        };
        for (const std::string& dir : extraIncludeDirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        for (const std::string& dir : extraLibDirs) {
            args.push_back("-L");
            args.push_back(dir);
        }
        int rc = ebasic::runProcess(args);
        if (rc != 0) return rc;
    }
    return 0;
}

} // namespace ebpm
