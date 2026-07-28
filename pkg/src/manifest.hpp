#pragma once

#include <string>
#include <vector>

namespace ebpm {

// A `[dependencies]` entry: exactly one of `path`/`git` is set (mutual
// exclusion enforced by the loader, not by this struct's shape). At most one
// of `branch`/`tag`/`rev` is meaningful, and only when `git` is set - M5a's
// loader accepts and stores all three; only M5d's resolver actually acts on
// them.
struct Dependency {
    std::string name;
    std::string path;
    std::string git;
    std::string branch;
    std::string tag;
    std::string rev;
};

// A package's `[lib]` target - present iff the package builds a library.
// `path` defaults to "src/lib.bas" if the manifest omits it.
struct LibTarget {
    std::string path;
};

// A package's `[bin]` target - present iff the package builds an
// executable. `name` defaults to `Manifest::name`, `path` to "src/main.bas",
// if the manifest omits either.
struct BinTarget {
    std::string name;
    std::string path;
};

// The parsed, defaults-applied contents of an `ebasic.toml` manifest. A
// package may have a `[lib]`, a `[bin]`, both, or (rejected by the loader)
// neither.
struct Manifest {
    std::string name;
    std::string version; // metadata only for M5 - never constraint-solved
    bool hasLib = false;
    LibTarget lib;
    bool hasBin = false;
    BinTarget bin;
    std::vector<Dependency> dependencies;
};

// Parses and validates `manifestPath` (usually "<dir>/ebasic.toml") into
// `out`, applying every field default described above. Returns false (with
// a human-readable message in `err`) on a missing file, malformed TOML, a
// missing required field, or a `[dependencies]` entry naming neither `path`
// nor `git` (or both).
bool loadManifest(const std::string& manifestPath, Manifest& out, std::string& err);

} // namespace ebpm
