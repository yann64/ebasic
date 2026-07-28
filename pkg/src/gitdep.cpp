#include "gitdep.hpp"
#include "driver/process.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace ebpm {

namespace {

// A filesystem-safe cache-directory name for a git URL - every character
// that isn't alphanumeric/`-`/`_`/`.` becomes `_`. Deliberately readable
// rather than an opaque hash, so `~/.ebpm/cache/git/` stays inspectable by
// hand.
std::string sanitizeForDirName(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') {
            r.push_back(c);
        } else {
            r.push_back('_');
        }
    }
    return r;
}

fs::path gitCacheRoot() {
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".ebpm" / "cache" / "git";
}

std::string trimTrailingNewline(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

} // namespace

bool resolveGitDependency(const Dependency& dep, const std::string& pinnedCommit,
                          std::string& resolvedDir, std::string& resolvedCommit, std::string& err) {
    fs::path cacheDir = gitCacheRoot() / sanitizeForDirName(dep.git);
    std::error_code ec;
    fs::create_directories(gitCacheRoot(), ec);

    bool alreadyCloned = fs::exists(cacheDir / ".git", ec);
    if (!alreadyCloned) {
        std::cerr << "    Cloning " << dep.git << std::endl;
        if (ebasic::runProcess({"git", "clone", dep.git, cacheDir.string()}) != 0) {
            err = "failed to clone '" + dep.git + "'";
            return false;
        }
    } else {
        std::cerr << "    Fetching " << dep.git << std::endl;
        if (ebasic::runProcess({"git", "-C", cacheDir.string(), "fetch"}) != 0) {
            err = "failed to fetch '" + dep.git + "'";
            return false;
        }
    }

    // A pinned commit (from a prior ebasic.lock entry) always wins - this
    // is what keeps a repeat build reproducible even if the remote branch
    // named by `dep.branch` has since moved. Otherwise fall back to
    // whatever ref the manifest itself names.
    std::string ref;
    bool isBranch = false;
    if (!pinnedCommit.empty()) {
        ref = pinnedCommit;
    } else if (!dep.branch.empty()) {
        ref = dep.branch;
        isBranch = true;
    } else if (!dep.tag.empty()) {
        ref = dep.tag;
    } else if (!dep.rev.empty()) {
        ref = dep.rev;
    }
    if (!ref.empty()) {
        // A remote branch has no local tracking branch on a fresh clone -
        // try the remote-tracking form first, but *only* for `branch`
        // (the only case "origin/<ref>" could ever mean anything - a
        // pinned commit SHA or a tag is never spelled that way, and
        // trying it there would just print a confusing, guaranteed-to-fail
        // attempt before the real, correct checkout below).
        int rc = -1;
        if (isBranch) {
            rc = ebasic::runProcess({"git", "-C", cacheDir.string(), "checkout", "origin/" + ref});
        }
        if (rc != 0) {
            rc = ebasic::runProcess({"git", "-C", cacheDir.string(), "checkout", ref});
        }
        if (rc != 0) {
            err = "failed to checkout '" + ref + "' for '" + dep.git + "'";
            return false;
        }
    }

    std::string output;
    if (ebasic::runProcessCaptureOutput({"git", "-C", cacheDir.string(), "rev-parse", "HEAD"},
                                         output) != 0) {
        err = "failed to resolve the current commit for '" + dep.git + "'";
        return false;
    }
    resolvedCommit = trimTrailingNewline(output);
    resolvedDir = cacheDir.string();
    return true;
}

} // namespace ebpm
