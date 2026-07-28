# `ebc` - the compiler

```
usage: ebc <input.bas> [-o <output>] [-cxx <compiler>] [-L <dir>]... [-I <dir>]...
           [-l <name>]... [--keep-cpp] [--lib]
       ebc [-v | --version] [-h | --help]
```

Transpiles `<input.bas>` to C++ and invokes a real backend compiler to
produce a native executable (or, with `--lib`, a static library).

## Flags

- **`-o <output>`** - output path. Defaults to the input file's own stem
  (`hello.bas` -> `hello`) if omitted.
- **`-cxx <compiler>`** - which backend compiler to invoke. Defaults to the
  `CXX` environment variable if set, else the bare name `g++` (resolved via
  `PATH`). `clang++` is a real, tested alternative (`-cxx clang++` or
  `CXX=clang++`) - verified to compile and run the entire language test
  suite, including `--lib` mode and `EXTERN` interop, and verified that a
  library built with one of `g++`/`clang++` links correctly against a
  program built with the other. This is independent of which compiler built
  `ebc` itself (see [Developer Documentation](../developer/architecture.md)
  if you're building eBasic from source) - `ebc` always uses whatever
  `-cxx`/`CXX` says, regardless.
- **`-L <dir>`** (repeatable) - extra library search path, forwarded to the
  backend as `-L <dir>`.
- **`-I <dir>`** (repeatable) - extra `#include` search path for `.bas`
  source, consulted only as a fallback *after* the normal
  includer-relative lookup fails (so a package can depend on another
  package's directory - e.g. an auto-generated `.iface.bas` - without
  knowing its exact relative filesystem path).
- **`-l <name>`** (repeatable) - extra library to link (`-l<name>`),
  alongside anything already named by the module's own `Lib "name"`
  clauses (see [`EXTERN` interop](../reference/extern-interop.md)). Needed
  for a *transitive* dependency whose own `Lib` clause never appears in the
  compiled module directly.
- **`--keep-cpp`** - don't delete the generated `<output>.gen.cpp`
  intermediate file (useful for inspecting what eBasic actually generates).
- **`--lib`** - build a library instead of an executable (see below).
- **`-v` / `--version`** - print the version and exit.
- **`-h` / `--help`** - print usage and exit.

## `--lib` mode

```sh
$ ebc mylib.bas --lib -o mylib
```

`mylib.bas` may only contain declarations (`DIM`/`CONST`/`ENUM`/`SUB`/
`FUNCTION`/`TYPE`/`UNION`/`NAMESPACE`) at the top level - no executable
statement, since a library's object file must never define `main` itself
(it would collide with a consuming program's own `main` at final link
time). Produces two files:

- **`lib<output>.a`** - a real static archive.
- **`<output>.iface.bas`** - an auto-generated interface file: an
  `Extern "C++" Lib "<output>" ... End Extern` block with one `Declare` per
  public top-level `SUB`/`FUNCTION` (aliased to its real mangled symbol),
  plus a verbatim copy of any plain-data or opaque `TYPE`/`UNION`. A
  dependent program `#include`s this file and links the archive - reusing
  the same [`EXTERN`/`DECLARE`](../reference/extern-interop.md) machinery
  any `.bas` program already uses to call a C/C++ library, rather than a
  separate cross-package mechanism.

This is exactly the mechanism [`ebpm`](ebpm.md) uses under the hood for a
package's `[lib]` target - see that page for the higher-level, manifest-driven
workflow (dependency resolution, transitive linking, lockfiles) built on top
of it.

## See also

- [Getting Started](getting-started.md)
- [`ebpm`](ebpm.md)
- [`EXTERN` / C-C++ Interop](../reference/extern-interop.md)
