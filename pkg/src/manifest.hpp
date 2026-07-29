#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ebpm {

/// A `[dependencies]` entry: exactly one of `path`/`git`/`version` is set
/// (mutual exclusion enforced by the loader, not by this struct's shape).
/// At most one of `branch`/`tag`/`rev` is meaningful, and only when `git`
/// is set (never combined with `version` - a registry index entry already
/// names its own tag, see loadManifest's doc comment). `version` is a
/// requirement string (e.g. `"^1.2.0"`) identifying a *registry*
/// dependency - `name = "^1.2.0"` is shorthand for `name = { version =
/// "^1.2.0" }`, matching Cargo's own `serde = "1.0"` shorthand.
struct Dependency {
    std::string name;
    std::string path;
    std::string git;
    std::string branch;
    std::string tag;
    std::string rev;
    std::string version;
};

/// A package's `[lib]` target - present iff the package builds a library.
/// `path` defaults to "src/lib.bas" if the manifest omits it.
struct LibTarget {
    std::string path;
};

/// A package's `[bin]` target - present iff the package builds an
/// executable. `name` defaults to `Manifest::name`, `path` to "src/main.bas",
/// if the manifest omits either.
struct BinTarget {
    std::string name;
    std::string path;
};

/// The parsed, defaults-applied contents of an `ebasic.toml` manifest. A
/// package may have a `[lib]`, a `[bin]`, both, or (rejected by the loader)
/// neither.
struct Manifest {
    std::string name;
    std::string version; // metadata only for M5 - never constraint-solved
    bool hasLib = false;
    LibTarget lib;
    bool hasBin = false;
    BinTarget bin;
    std::vector<Dependency> dependencies;
    /// `[target.<os>.dependencies]` entries, keyed by the exact platform name
    /// used throughout this project's own CMakePresets.json/CI (`"windows"`/
    /// `"linux"`/`"macos"`/`"haiku"`) - not FreeBASIC's own macro spelling
    /// (`__FB_WIN32__`/...), a deliberate, separate vocabulary: this is a
    /// human-typed TOML key, not source-level FreeBASIC-compatibility text.
    /// Only the current platform's own entry (if any) is ever built - see
    /// effectiveDependencies() and currentTargetOS().
    std::unordered_map<std::string, std::vector<Dependency>> targetDependencies;
};

/// The exact platform name `[target.<os>.dependencies]` sections are keyed
/// by for *this* build of ebpm - one of `"windows"`/`"linux"`/`"macos"`/
/// `"haiku"`, computed the same way at compile time as every other
/// platform-detection call site in this project (`process.cpp`,
/// `gitdep.cpp`). ebpm never cross-compiles - "the current target" always
/// means "whatever OS this ebpm binary itself is running on."
std::string currentTargetOS();

/// `manifest.dependencies` (built on every platform) plus
/// `manifest.targetDependencies[currentTargetOS()]` (built only on this
/// platform, empty if the manifest has no matching `[target.*]` section) -
/// the single, shared merge every real dependency-walking call site
/// (`resolveDependencyGraph` in resolve.cpp, `collectTransitiveDeps` in
/// build.cpp) uses, so "which dependencies actually apply" is computed in
/// exactly one place.
std::vector<Dependency> effectiveDependencies(const Manifest& manifest);

/// Parses and validates `manifestPath` (usually "<dir>/ebasic.toml") into
/// `out`, applying every field default described above. Returns false (with
/// a human-readable message in `err`) on a missing file, malformed TOML, a
/// missing required field, a `[dependencies]` (or `[target.*.dependencies]`)
/// entry naming zero or more than one of `path`/`git`/`version`, `version`
/// combined with `branch`/`tag`/`rev`, a malformed `version` requirement
/// string, or a `[target.X...]` section whose `X` isn't one of
/// `windows`/`linux`/`macos`/`haiku`.
bool loadManifest(const std::string& manifestPath, Manifest& out, std::string& err);

} // namespace ebpm
