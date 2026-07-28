#pragma once

#include "sema/sema.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ebasic::lsp {

/// Walks up from `startDir` (a document's own directory) looking for
/// `ebasic.toml`, the same way `ebpm` itself finds "the package rooted at
/// the current directory" - just anchored at the file's own directory
/// instead of a process's CWD. Returns the (canonical) directory
/// containing it, or nullopt if none is found before the filesystem root.
std::optional<std::string> findPackageRoot(const std::string& startDir);

/// One dependency's own auto-generated interface, parsed once - reused for
/// cross-package hover/go-to-definition without re-parsing on every
/// request. `path` is the real, on-disk `.iface.bas` location (used as the
/// target URI for a go-to-definition landing in it).
struct DependencyInterface {
    std::string path;
    ebasic::SemaIndex index;
};

/// Everything an `ebpm` package's own document needs for `#include`
/// resolution and cross-package symbol lookups - computed once per package
/// root via `ebpm`'s own real dependency-graph resolution (`ebasic_pkg`'s
/// `resolveDependencyGraph`/`computeConsumerDirs`), **not** recomputed per
/// keystroke: a git dependency's resolution runs a real `git fetch` every
/// time, so this must be cached by the caller (see `Server`'s own
/// `packageCache_`) and only refreshed on `didOpen` or an explicit
/// invalidation (`workspace/didChangeWatchedFiles`), never on `didChange`.
struct PackageContext {
    /// The `-I` search path `ebc`/`ebpm build` would use for this
    /// package's own source - `computeConsumerDirs`'s result (which also
    /// includes the package's own target dir when it has a [lib] target;
    /// harmless here since a package's own source never needs to
    /// `#include` itself).
    std::vector<std::string> includeDirs;
    /// Canonical names of every effective dependency whose own
    /// `target/<name>.iface.bas` doesn't exist yet (never built) - lets a
    /// raw "cannot open included file" diagnostic be turned into an
    /// actionable "run `ebpm build`" hint instead.
    std::vector<std::string> missingInterfaces;
    /// Every *built* dependency's own parsed interface, keyed by package
    /// name - consulted for hover/go-to-definition when a symbol isn't
    /// found in the current document's own SemaIndex.
    std::unordered_map<std::string, DependencyInterface> dependencies;
};

/// Resolves `packageDir`'s full dependency graph (a real `ebpm`-equivalent
/// resolution - clones/fetches a git dependency exactly like `ebpm build`
/// would) and builds a `PackageContext` from it: `computeConsumerDirs` for
/// `includeDirs`, `interfacePath` existence per dependency for
/// `missingInterfaces`, and parses each *existing* interface (via the same
/// Preprocessor -> Lexer -> Parser -> Sema pipeline `checkDocument` uses)
/// into `dependencies`. A resolution failure (a cycle, an unresolvable
/// path, ...) yields an empty `PackageContext` rather than propagating an
/// error - the caller degrades to "no package awareness" instead of
/// failing outright.
PackageContext resolvePackageContext(const std::string& packageDir);

} // namespace ebasic::lsp
