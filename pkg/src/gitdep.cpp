#include "gitdep.hpp"
#include "driver/process.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace ebpm {

std::string homeDir() {
    /// M8a: Windows doesn't set HOME by default (some shells/environments
    /// do, but it isn't guaranteed) - USERPROFILE is its real equivalent,
    /// checked as a fallback rather than assumed unnecessary.
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    return home ? home : ".";
}

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

bool cloneOrFetch(const std::string& url, const std::string& cacheDir, std::string& err) {
    std::error_code ec;
    bool alreadyCloned = fs::exists(fs::path(cacheDir) / ".git", ec);
    if (!alreadyCloned) {
        std::cerr << "    Cloning " << url << std::endl;
        if (ebasic::runProcess(ebasic::hostExecArgs({"git", "clone", url, cacheDir})) != 0) {
            err = "failed to clone '" + url + "'";
            return false;
        }
    } else {
        std::cerr << "    Fetching " << url << std::endl;
        if (ebasic::runProcess(ebasic::hostExecArgs({"git", "-C", cacheDir, "fetch"})) != 0) {
            err = "failed to fetch '" + url + "'";
            return false;
        }
    }
    return true;
}

namespace {

/// The global cache root every git dependency is cloned/fetched into
/// (`<home>/.ebpm/cache/git/`), shared across every package on the machine
/// rather than per-package - the same URL cloned by two different packages
/// reuses one clone.
fs::path gitCacheRoot() { return fs::path(homeDir()) / ".ebpm" / "cache" / "git"; }

std::string trimTrailingNewline(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

} // namespace

bool resolveGitDependency(const Dependency& dep, const std::string& pinnedCommit,
                          std::string& resolvedDir, std::string& resolvedCommit, std::string& err) {
    /// Cache-directory key: the URL, plus (if the manifest names one) the
    /// declared branch/tag/rev - deliberately NOT the pinned commit, which
    /// differs between an unpinned first resolution and a pinned repeat
    /// build of the exact same edge and would otherwise force a needless
    /// re-clone the moment a lockfile pin appears. Widening the key at all
    /// (beyond the URL alone, as before) fixes a real collision: two
    /// different dependency edges naming the *same* URL at *different*
    /// refs (e.g. two versions of one registry-published package, tagged
    /// v1.0.0/v1.1.0 in one repo) used to share a single mutable checkout,
    /// so the second edge's checkout silently mutated the first edge's
    /// already-resolved working tree out from under it - never exercised
    /// by any single-ref e2e case until the registry made it likely.
    std::string selector = !dep.branch.empty() ? dep.branch : (!dep.tag.empty() ? dep.tag : dep.rev);
    std::string cacheKey = dep.git + (selector.empty() ? "" : ("@" + selector));
    fs::path cacheDir = gitCacheRoot() / sanitizeForDirName(cacheKey);
    std::error_code ec;
    fs::create_directories(gitCacheRoot(), ec);

    if (!cloneOrFetch(dep.git, cacheDir.string(), err)) return false;

    /// A pinned commit (from a prior ebasic.lock entry) always wins - this
    /// is what keeps a repeat build reproducible even if the remote branch
    /// named by `dep.branch` has since moved. Otherwise fall back to
    /// whatever ref the manifest itself names.
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
        /// A remote branch has no local tracking branch on a fresh clone -
        /// try the remote-tracking form first, but *only* for `branch`
        /// (the only case "origin/<ref>" could ever mean anything - a
        /// pinned commit SHA or a tag is never spelled that way, and
        /// trying it there would just print a confusing, guaranteed-to-fail
        /// attempt before the real, correct checkout below).
        int rc = -1;
        if (isBranch) {
            rc = ebasic::runProcess(
                ebasic::hostExecArgs({"git", "-C", cacheDir.string(), "checkout", "origin/" + ref}));
        }
        if (rc != 0) {
            rc = ebasic::runProcess(
                ebasic::hostExecArgs({"git", "-C", cacheDir.string(), "checkout", ref}));
        }
        if (rc != 0) {
            err = "failed to checkout '" + ref + "' for '" + dep.git + "'";
            return false;
        }
    }

    std::string output;
    if (ebasic::runProcessCaptureOutput(
            ebasic::hostExecArgs({"git", "-C", cacheDir.string(), "rev-parse", "HEAD"}), output) !=
        0) {
        err = "failed to resolve the current commit for '" + dep.git + "'";
        return false;
    }
    resolvedCommit = trimTrailingNewline(output);
    resolvedDir = cacheDir.string();
    return true;
}

} // namespace ebpm
