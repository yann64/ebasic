# eBasic

eBasic is an extended dialect of the BASIC programming language, transpiled
to C++ and compiled with a real backend compiler (g++/clang++/MSVC).

## Features

- Open source (MIT)
- Same syntax as FreeBASIC, but fully reimplemented from scratch
- Precompiled runtime header for faster compilation
- Reuse of real C/C++ headers/libraries (`EXTERN`/`DECLARE`)
- Cargo-like package management (`ebpm`), including OS-conditional
  dependencies (a different dependency per target platform, e.g. Win32 on
  Windows vs GTK4 on Linux) paired with auto-defined platform macros
  (`__FB_WIN32__`/`__FB_LINUX__`/`__FB_DARWIN__`/`__FB_HAIKU__`) for
  conditional compilation in `.bas` source itself
- Namespaces, TYPE-based OOP (fields, inheritance, properties, operator overloading)
- Automated documentation generation (`docgen`)
- `ebpm`-aware Language Server (`ebasic_lsp`): real-time diagnostics,
  hover, go-to-definition, find-references, outline, and completion,
  resolving `#include`d `ebpm` dependency interfaces the same way a real
  `ebpm build` would

## OS Support

Linux, Windows, macOS, Haiku - all four verified in CI (Linux/macOS/Windows)
and live on real hardware (Haiku).

## Status

`1.4.0`. All phased milestones (M0-M8: core language, C/C++ interop, `ebpm`,
precompiled runtime header, `docgen`, four-platform ports) are complete,
along with post-M8 CLI ergonomics, Linux packaging (`.deb`/`.rpm`/Flatpak),
OS-conditional dependencies, (M9) FreeBASIC-parity preprocessor directives
(`#elseif`/`#if` expressions/`#macro`/stringize+concatenate/`__LINE__`/
`__FILE__`/`__DATE__`/`__TIME__`), MSVC backend support alongside
g++/clang++ (`windows-msvc` preset), and `Stdcall` calling-convention
support on `EXTERN`/`DECLARE` for Win32 APIs. See
[`docs/architecture/roadmap.md`](docs/architecture/roadmap.md) for the full
history and design decisions behind each milestone.

## Building

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Other presets (see `CMakePresets.json`): `linux-clang`, `windows-mingw`
(MinGW, run from an MSYS2 shell), `windows-msvc` (MSVC, run from an "x64
Native Tools Command Prompt"), `macos`, `haiku`.

## Installing (Linux)

Real, locally-buildable packages for all three major Linux packaging
formats - see each format's own directory for the build recipe:

- **`.deb`**: `dpkg-buildpackage -us -uc -b` (uses the top-level `debian/`)
- **`.rpm`**: `rpmbuild -bb packaging/rpm/ebasic.spec`
- **Flatpak**: `flatpak-builder --force-clean <builddir> packaging/flatpak/io.github.yann64.ebasic.json`

Every format installs the three tools plus the runtime header/PCH into the
platform's own standard prefix (`/usr` for `.deb`/`.rpm`, `/app` for
Flatpak) - `ebc` resolves its own runtime data relative to wherever it was
actually installed, so this works unmodified regardless of prefix.

## Usage

```sh
./build/linux-gcc/compiler/ebc examples/hello.bas -o hello
./hello
```

## Documentation

- **[Getting Started](docs/guide/getting-started.md)** - build, first program, first package
- **[Language Reference](docs/reference/index.md)** - every keyword, dictionary-style
- **End-user guide**: [`ebc`](docs/guide/ebc.md), [`ebpm`](docs/guide/ebpm.md), [`docgen`](docs/guide/docgen.md), [`ebasic_lsp`](docs/guide/lsp.md)
- **[`examples/`](examples/)** - one small, runnable program per language feature area
- **[Developer Documentation](docs/developer/architecture.md)** - compile pipeline, module map, for anyone extending the compiler itself
- **API docs** - generated with [Doxygen](https://www.doxygen.nl/) (`cmake --build build/linux-gcc --target docs`, output at `build/docs/html/index.html`)
- **[`docs/architecture/roadmap.md`](docs/architecture/roadmap.md)** - the historical design/decision log, one section per milestone
