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
tests/           e2e golden tests (one directory per feature area) + fixtures
scripts/         dev-facing scripts (Haiku verification, PCH benchmarking)
packaging/haiku/ draft HaikuPorts recipe
docs/            this documentation
examples/        example .bas programs
```

Each of `compiler/`, `runtime/`, `pkg/`, `docgen/`, `tests/` has its own
`CMakeLists.txt`, included from the top-level one.

## Shared libraries: why `ebc`, `ebpm`, and `docgen` aren't three silos

Two small static libraries (`compiler/CMakeLists.txt`) exist purely to avoid
duplicating code across the three binaries:

- **`ebasic_process`** (`compiler/src/driver/process.cpp`): the
  cross-platform subprocess execution helper (`runProcess`,
  `runProcessCaptureOutput`) - POSIX (`fork`/`execvp`/`waitpid`/`pipe`) or
  Windows (`CreateProcess`) under the hood, picked at compile time. `ebc`
  needs it to invoke the backend compiler; `ebpm` needs it for the same
  reason (building each package) plus to shell out to `git` for git
  dependencies.
- **`ebasic_frontend`**: the Preprocessor/Lexer/Parser/Diagnostics quartet,
  with Sema and Codegen deliberately excluded. `docgen` links this to get a
  real, parsed `Module` - it needs the exact same parsing behavior `ebc`
  itself uses (no second, drifting parser implementation), but never needs
  type-checking or code generation, since every declaration it documents is
  already structurally resolved by the parser alone.

Both mirror the same shape: a `STATIC` library with
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
This is GCC-only by design: Clang needs an explicit `-include-pch` flag and
simply never notices the `.gch`, so `-cxx clang++` still works, just without
the speedup. `scripts/bench_pch.sh` measures the actual win.

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
