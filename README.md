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

eBasic is under active early development. See `docs/architecture/` for the compiler
architecture and phased roadmap. The current milestone (M0) implements a minimal
pipeline: `DIM`, `PRINT`, assignment, and arithmetic/string-concatenation expressions,
compiled to C++ and built via `g++`/`clang++`.

## Building

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

## Usage

```sh
./build/linux-gcc/compiler/ebc examples/hello.bas -o hello
./hello
```
