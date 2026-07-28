# Getting Started

## Building from source

eBasic ships as source, built with CMake:

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

This produces three binaries under `build/linux-gcc/`: `compiler/ebc`,
`pkg/ebpm`, `docgen/docgen`. Other presets exist for `linux-clang`,
`windows-mingw` (MinGW, run from an MSYS2 shell), `macos`, and `haiku` - see
`CMakePresets.json`.

Every binary reports its own version:

```sh
$ build/linux-gcc/compiler/ebc --version
ebc 0.1.0 (a37c8bf)
```

(The parenthetical is the exact git commit the binary was built from - empty
if built from a source tree with no `.git`, e.g. a packaged release.)

## Your first program

A `.bas` file is transpiled to C++ and compiled by `ebc` in one step:

```basic
' hello.bas
PRINT "Hello, world!"
```

```sh
$ ebc hello.bas -o hello
$ ./hello
Hello, world!
```

`ebc` invokes `g++` (or `$CXX`, or `-cxx <compiler>`) as a subprocess to
produce the final native executable - see the
[`ebc` reference page](ebc.md) for every flag, and the
[language reference](../reference/index.md) for the language itself.

## Multi-file / multi-package projects

For anything beyond a single file, `ebpm` (eBasic's package manager, in the
spirit of Cargo) scaffolds and builds a package:

```sh
$ ebpm new hello_pkg
Created binary package 'hello_pkg' at hello_pkg
$ cd hello_pkg
$ ebpm run
   Compiling hello_pkg (bin)
Hello from hello_pkg!
```

See the [`ebpm` reference page](ebpm.md) for manifest format, dependencies,
and every command.

## Documenting your own code

A `'''`-marked comment on a `SUB`/`FUNCTION`/`TYPE`/`UNION`/`NAMESPACE`/
`CONST`/`ENUM` is extracted by `docgen` into Markdown/HTML - see the
[doc-comments reference](../reference/doc-comments.md) and the
[`docgen` reference page](docgen.md).

## Where to go next

- [Language Reference](../reference/index.md) - every keyword, dictionary-style
- [`ebc`](ebc.md), [`ebpm`](ebpm.md), [`docgen`](docgen.md) - tool usage
- [`examples/`](../../examples/) - small, runnable programs, one per feature area
- [Developer Documentation](../developer/architecture.md) - if you're extending the compiler itself
