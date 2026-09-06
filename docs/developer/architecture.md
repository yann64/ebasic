# eBasic Architecture

This is a current-state overview of how eBasic is put together, for anyone
extending the compiler, `ebpm`, or `docgen`. It describes what exists today;
for the history of *how* it got here (design decisions, alternatives
considered, verification notes per milestone), see
[`docs/architecture/roadmap.md`](../architecture/roadmap.md). For per-symbol
API details (exact function signatures, class members), see the
[Doxygen-generated API docs](#generated-api-docs) - this page stays narrative
and doesn't duplicate that content.

## The compile pipeline

`ebc` (and `docgen`, for the front-end half) transpiles a `.bas` file to C++
and hands it to a real backend compiler:

```
.bas source
    |
    v
Preprocessor   (#define, #ifdef/#ifndef/#else/#endif, #include/#include once)
    |
    v
Lexer          (source text -> flat token stream)
    |
    v
Parser         (tokens -> Module AST, recursive descent)
    |
    v
Sema           (name resolution + type checking, in place on the Module)
    |
    v
Codegen        (Module -> a single C++ translation unit's worth of text)
    |
    v
g++ / clang++  (the real backend compiler, invoked as a subprocess)
    |
    v
native executable, static library, or object file
```

Each stage is a distinct, independently testable component under
`compiler/src/`:

| Stage | Directory | Key type(s) |
|---|---|---|
| Preprocessor | `compiler/src/preprocessor/` | `PreprocessResult` |
| Lexer | `compiler/src/lexer/` | `Token`, `TokenKind`, `Lexer` |
| Parser | `compiler/src/parser/` | `Parser`, produces a `Module` (see `compiler/src/ast/ast.hpp`) |
| Sema | `compiler/src/sema/` | `Sema` - checks a `Module` in place |
| Codegen | `compiler/src/codegen/` | `Codegen` - lowers a `Module` to C++ text |
| Diagnostics | `compiler/src/diagnostics/` | `DiagnosticEngine`, `Diagnostic`, `SourceLoc` |
| Driver | `compiler/src/driver/` | `main.cpp` (ties the pipeline together + invokes the backend), `process.cpp` (cross-platform subprocess execution) |

A `SourceLoc` carries a `fileId` (registered once per physical file via
`DiagnosticEngine::registerFile`) alongside line/column, so a diagnostic from
deep inside a `#include`d file - or from source that's been macro-expanded
and no longer matches any single file's line numbers 1:1 - still reports its
true originating location. The Preprocessor's `PreprocessResult::lineMap`
carries this mapping forward into the Lexer.

## Directory / component map

```
compiler/src/    the compiler front end + back end (see pipeline above)
runtime/include/ the C++ runtime backing built-in types (BString, printLine, ...)
pkg/src/         ebpm - the package manager
docgen/src/      docgen - doc-comment ('''-marked) -> Markdown/HTML
lsp/src/         ebasic_lsp - the language server (see docs/guide/lsp.md)
tests/           e2e golden tests (one directory per feature area) + fixtures
scripts/         dev-facing scripts (Haiku verification, PCH benchmarking)
packaging/haiku/ draft HaikuPorts recipe
docs/            this documentation
examples/        example .bas programs
```

Each of `compiler/`, `runtime/`, `pkg/`, `docgen/`, `tests/` has its own
`CMakeLists.txt`, included from the top-level one.

## Shared libraries: why `ebc`, `ebpm`, `docgen`, and `ebasic_lsp` aren't four silos

Small static libraries exist purely to avoid duplicating code across the
four binaries:

- **`ebasic_process`** (`compiler/CMakeLists.txt`, `compiler/src/driver/process.cpp`):
  the cross-platform subprocess execution helper (`runProcess`,
  `runProcessCaptureOutput`) - POSIX (`fork`/`execvp`/`waitpid`/`pipe`) or
  Windows (`CreateProcess`) under the hood, picked at compile time. `ebc`
  needs it to invoke the backend compiler; `ebpm` needs it for the same
  reason (building each package) plus to shell out to `git` for git
  dependencies.
- **`ebasic_frontend`** (`compiler/CMakeLists.txt`): the Preprocessor/
  Lexer/Parser/Diagnostics quartet, with Sema and Codegen deliberately
  excluded. `docgen` links this to get a real, parsed `Module` - it needs
  the exact same parsing behavior `ebc` itself uses (no second, drifting
  parser implementation), but never needs type-checking or code
  generation, since every declaration it documents is already structurally
  resolved by the parser alone.
- **`ebasic_sema`** (`compiler/CMakeLists.txt`): `Sema` on its own, split
  out the same way for the same reason - `ebasic_lsp` needs *real*,
  resolved symbol/type information (for hover, go-to-definition, and
  diagnostics that match `ebc` exactly) but never needs Codegen, since it
  never emits C++.
- **`ebasic_pkg`** (`pkg/CMakeLists.txt`): `ebpm`'s manifest parsing,
  dependency-graph resolution, and build-flag computation
  (`manifest.cpp`/`resolve.cpp`/`build.cpp`/`lockfile.cpp`/`gitdep.cpp`),
  split out so `ebasic_lsp` can resolve a workspace's `ebpm` dependencies
  (`#include "dep.iface.bas"`) the exact same way a real `ebpm build`
  would, in-process - not a second, potentially-drifting reimplementation
  of dependency resolution.

Every one of these mirrors the same shape: a `STATIC` library with
`target_include_directories(... PUBLIC ...)` so a consumer can `#include`
the relevant headers directly.

## The M6 precompiled runtime header

`ebc` compiles generated C++ against `runtime/include/ebasic/runtime/*.hpp`
on every invocation. `runtime/CMakeLists.txt` precompiles
`runtime.hpp` into a GCC `.gch` at build-tree configure time
(`${CMAKE_BINARY_DIR}/runtime_pch/ebasic/runtime/runtime.hpp.gch`, built by
literally invoking `g++` - not `${CMAKE_CXX_COMPILER}`, since `ebc`'s own
driver always shells out to plain `g++`/`$CXX` regardless of which compiler
built `ebc` itself).

`ebc` passes this PCH directory as an extra `-I` *before* the real runtime
include directory (see `runtimeIncludeArgs()` in
`compiler/src/driver/main.cpp`) - GCC automatically prefers a same-named
`.gch` sitting in an earlier `-I` entry over reparsing the real header, with
no special flags needed at the call site, and falls back silently (correct,
just slower) if the `.gch` doesn't match the invoking compiler's version.
This is GCC-specific behavior: Clang needs an explicit `-include-pch` flag
and simply never notices the `.gch`, so `-cxx clang++` still works, just
without the speedup. `scripts/bench_pch.sh` measures the actual win.

**MSVC has a real, parallel PCH rule too** (not just a graceful no-op the
way Clang gets): `runtime/CMakeLists.txt` also precompiles a dedicated PCH
source file (`runtime/pch/runtime_pch.cpp`, whose only content is
`#include "ebasic/runtime/runtime.hpp"` - `cl.exe` can't precompile a bare
header the way `g++ -x c++-header` can) via `cl /Ycebasic/runtime/runtime.hpp
/Fp<path>\runtime.pch`, into the same `runtime_pch/ebasic/runtime/`
directory the GCC `.gch` uses (different filename, no collision - only one
toolchain's file is ever relevant to a given `ebc` invocation).
`runtimeIncludeArgs()` passes the matching `/Yu<header>`/`/Fp<path>` flags
whenever `-cxx`/`CXX` names real `cl` and a real `.pch` exists for this
build - **never `clang-cl`** (see below: MSVC's PDB-based PCH format and
Clang's own AST-based one aren't interchangeable, so the `.pch` a real
`cl.exe` built here would be useless, or worse, to `clang-cl`). Unlike
GCC, MSVC has **no automatic lookup and no graceful
fallback**: `/Yu`/`/Fp` are mandatory explicit flags, and a stale/
mismatched `.pch` is a *hard* compile error (`C1852` "is not a valid
precompiled header file" confirmed live against real `cl.exe`; `C1010`/
`C1083`/`C2859` are Microsoft's other documented codes for the same
failure family) rather than a silent reparse. `main.cpp`'s
`runCompilerStepWithPchFallback` reproduces GCC's own "always correct,
just slower if unavailable" guarantee explicitly: it retries the compile
once without the PCH flags whenever the first attempt's captured output
matches one of those codes.

MSVC also has one genuine, MSVC-specific requirement GCC's `.gch` never
needed: the PCH-creation compile always produces a companion object file
(`runtime_pch.obj`) that **must be present in the final link** whenever
any input object was compiled with `/Yu` against the same `.pch` - a real
MSVC linker requirement (`LNK2011` "precompiled object missing from the
link" otherwise), discovered empirically against real `cl.exe`, not
something anticipated up front. `main.cpp`'s `msvcRuntimePchObjectPath()`
derives that object's path from an already-built `runtimeIncludes` list's
`/Fp` token.

**`--lib` mode uses the PCH too**, closing what was originally a
deliberate carve-out: that mode's static-archive output is consumed by a
*separate*, later `ebc` invocation (whatever links against it), and MSVC
just needs *some* copy of the matching PCH-creation object present
anywhere in *that* final link - not specifically one produced by the
invocation that compiled the archived object. `main.cpp`'s
`msvcRuntimePchObjectIfAvailable()` makes every plain-executable and
`--shared-lib` link **defensively** include the PCH object whenever one
exists right now, independent of whether *that specific* invocation's own
compile happened to use `/Yu` (it may have fallen back to no-PCH on a
mismatch, or simply have nothing to do with whether a linked-in static
archive needs it) - the object is inert, so including it "just in case"
costs nothing on the many links where nothing actually needed it.
Confirmed live: a `--lib` archive built with PCH links and runs correctly
from a consuming executable, including when that consuming executable's
own compile falls back to no-PCH (a corrupted `.pch`) while the archive's
own PCH-compiled object still needs the companion object present.

This makes PCH-in-`--lib`-mode robust *within one consistent build
environment* (the real-world case: one `ebpm build` run, one `ebc`
install, PCH available throughout) - it does not, and cannot cheaply,
solve the cross-environment case (a `--lib` archive built with PCH
available, later linked where PCH isn't available at all) - an inherent
limitation of MSVC's PCH model for any prebuilt artifact, not unique to
this mechanism.

### `clang-cl` as a genuinely verified backend option

`isMsvcToolchain()` matches `clang-cl` (Clang's own MSVC-compatible
driver mode) alongside `cl` for flag-syntax purposes - `/std:c++17`,
`/EHsc`, `/c`, `/Fo:`/`/Fe:`, `/link`/`/LIBPATH:`, `lib.exe` archiving,
all shared, by LLVM's own design goal of drop-in `cl.exe` compatibility.
Verified live against a real `clang-cl.exe`: a trivial program, an
`EXTERN`/`Stdcall` case, a `TYPE`-field/`PROPERTY` function-pointer case,
and a `--lib`+consuming-exe round trip (the last two also proving
`lib.exe` archiving and `/LIBPATH:` linking work unchanged against a
`clang-cl`-compiled object) all compile, link, and run with correct
output - as does the *entire* existing test suite with `CXX=clang-cl`
forced as the backend for every `ebc` invocation.

**`clang-cl` never gets the MSVC PCH speedup, by design**: `isClangCl()`
(`compiler/src/driver/main.cpp`) gates PCH usage off specifically for
`clang-cl` even though `isMsvcToolchain()` is still true - the only
`.pch` that could ever exist was built by real `cl.exe`
(`runtime/CMakeLists.txt`'s MSVC PCH block), and MSVC's PDB-based PCH
serialization isn't interchangeable with Clang's own AST-based one.
`clang-cl` gets the same graceful "no PCH available, just slower" path
every other unsupported-PCH case already gets - no `/Yu`/`/Fp` is ever
emitted, so none of the fallback/retry/companion-object machinery above
is ever exercised for it either.

## Relocatable installs (M8e)

`ebc` doesn't hardcode a single path to its runtime headers/PCH. At startup,
`resolveOwnExecutablePath()` (`compiler/src/driver/main.cpp`) resolves its
own on-disk location from `argv[0]` (handling both a path-containing name,
via `fs::canonical`, and a bare PATH-searched name, via a manual `PATH`
scan). `runtimeIncludeArgs()` then checks for an *installed* runtime
relative to that location first
(`<exeDir>/../<datadir>/ebasic/runtime/{include,pch}`, where the relative
path is baked in at configure time as `EBASIC_RUNTIME_INSTALL_RELDIR`),
falling back to the original compile-time-baked build-tree path if nothing
installed is found. This is what makes `cmake --install` produce a genuinely
relocatable `ebc` - not just for Haiku packaging, but as a real, general
feature: every dev/test workflow that runs `ebc` straight from the build
tree keeps working unchanged via the fallback.

The top-level `CMakeLists.txt` also `include(GNUInstallDirs)` and overrides
`CMAKE_INSTALL_DATADIR` to `data` (instead of the generic `share`) on the
`haiku` preset only (`CMakePresets.json`) - Haiku's real data directory is
`data`, confirmed on real hardware, and `GNUInstallDirs.cmake` has no
Haiku-specific handling of its own.

## Package management (`ebpm`)

`ebpm` (`pkg/src/`) reuses `ebc`'s own `--lib` mode rather than building a
separate cross-package type-checking pipeline: a library package compiles to
a real static archive (`lib<name>.a`) plus an auto-generated `.iface.bas`
interface file (re-exporting the library's public declarations as
`Extern "C++"`/`Declare` forms - the same EXTERN/DECLARE machinery every
`.bas` program can already use for any C/C++ library). A dependent package
`#include`s that interface file and links the archive; `ebpm` computes the
`-I`/`-L`/`-l` flags for a package's entire transitive dependency closure
(`pkg/src/build.cpp`'s `collectTransitiveDeps`), not just its direct
dependencies. TOML manifests (`ebasic.toml`) are parsed with a vendored
`third_party/tomlplusplus` (no network access needed to configure the
project). Path and git dependencies are both supported; `ebasic.lock` pins
git commits for reproducible repeat builds.

## Generated API docs

Per-symbol C++ API documentation (every class, function, and now-`///`
-commented declaration across `compiler/src`, `runtime/include`, `pkg/src`,
and `docgen/src`) is generated with [Doxygen](https://www.doxygen.nl/)
(config: `docs/Doxyfile`):

```sh
cmake --build build/linux-gcc --target docs
```

Output lands in `build/docs/html/index.html`.
