# eBasic

The eBasic compiler is an extented version of the BASIC programmming language that uses g++/clang++ as an intermediate compiler.

## Features

- Open sources (MIT)
- Same sythax as FreeBASIC, but fully reimplemented from scratch.
- Precompiled standard libs for faster compilation.
- Allow re-use of C/C++ headers/libs (will need to extend FreeBASIC synthax/keywords)
- Module management similar to cargo (rust).
- Support for Namespaces.
- Support for classes.
- Automated documentation generation support.

## OS Support

- Linux
- Windows
- macOS
- Haiku

## Status

eBasic is under active development. See `docs/architecture/` for the compiler
architecture and phased roadmap - M0 through M7 are complete (full core language,
C/C++ interop, the `ebpm` package manager, a precompiled runtime header, and
`docgen`); M8 (Windows/macOS/Haiku ports) is in progress.

## Building

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Other presets (see `CMakePresets.json`): `linux-clang`, `windows-mingw` (MinGW,
run from an MSYS2 shell), `macos`, `haiku` - the latter three are new in M8 and
verified primarily through CI rather than on this development machine (see
`docs/architecture/roadmap.md`'s M8 notes).

## Documentation

API documentation for the compiler's own C++ source is generated with
[Doxygen](https://www.doxygen.nl/) (config at `docs/Doxyfile`):

```sh
cmake --build build/linux-gcc --target docs
```

Output lands in `build/docs/html/index.html` (requires `doxygen`; `dot`
from Graphviz is used for class/include graphs if present).

## Usage

```sh
./build/linux-gcc/compiler/ebc examples/hello.bas -o hello
./hello
```
