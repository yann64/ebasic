# `ebc` - the compiler

```
usage: ebc <input.bas> [-o <output>] [-cxx <compiler>] [-L <dir>]... [-I <dir>]...
           [-l <name>]... [--keep-cpp] [--lib | --shared-lib]
       ebc [-v | --version] [-h | --help]
```

Transpiles `<input.bas>` to C++ and invokes a real backend compiler to
produce a native executable, a static library (`--lib`), or a real,
dynamically loadable shared library (`--shared-lib`/`-dll`).

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
- **`--shared-lib`** (alias **`-dll`**) - build a real, dynamically loadable
  shared library instead of an executable (see below). Mutually exclusive
  with `--lib`.
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
time). Produces three files:

- **`lib<output>.a`** - a real static archive.
- **`<output>.iface.bas`** - an auto-generated interface file: an
  `Extern "C++" Lib "<output>" ... End Extern` block with one `Declare` per
  public top-level `SUB`/`FUNCTION` (aliased to its real mangled symbol),
  plus a verbatim copy of any plain-data or opaque `TYPE`/`UNION`. A
  dependent program `#include`s this file and links the archive - reusing
  the same [`EXTERN`/`DECLARE`](../reference/extern-interop.md) machinery
  any `.bas` program already uses to call a C/C++ library, rather than a
  separate cross-package mechanism. Only public top-level `SUB`/`FUNCTION`
  and plain-data/opaque `TYPE`/`UNION` are exported this way - a `TYPE`'s
  own methods aren't (no equivalent mechanism exists yet for calling a
  method across this boundary), and neither is a re-declared (`Extern`,
  bodyless) `SUB`/`FUNCTION` (there's no new compiled code for it in the
  archive to point at) - a library wrapping a native C API should expose
  its public surface as top-level functions taking/returning its
  `TYPE`s, not as methods, until that gap closes. A `SUB`/`FUNCTION`
  whose parameters or return type use `STRING` is skipped too (an
  ordinary `STRING` compiles to a real `BString`, not the bare
  `const char*` an `Extern`-side re-declaration would assume - a genuine
  ABI mismatch, not just cosmetic) - `ebc` prints an `ebc: warning: ...`
  for each one skipped this way, and `.iface.bas` itself gets a matching
  comment; use `ZSTRING`/`ANY PTR` instead (see any of this project's
  own registry packages, e.g. `eb-gtk4`'s `TextBufferGetText`, for the
  usual pattern).
- **`<output>.libs`** - this archive's own `Lib "name"` clauses (M4), one
  per line, plain text. `.iface.bas` only ever names *this* archive itself
  (`Lib "<output>"`) - it has no way to also tell a dependent program
  about a *raw system* library this archive's own `Extern` declarations
  need (e.g. a GTK4 binding's `Lib "gtk-4"`), since that library
  contributes no new symbols to `<output>.a` for the interface generator
  to notice. `ebpm` reads this file for every dependency in the graph and
  forwards each name as an extra `-l` to the final consumer's own link
  step (see [`ebpm`](ebpm.md)) - a hand-invoked `ebc` build needs the
  equivalent passed manually via repeated `-l <name>` flags.

This is exactly the mechanism [`ebpm`](ebpm.md) uses under the hood for a
package's `[lib]` target - see that page for the higher-level, manifest-driven
workflow (dependency resolution, transitive linking, lockfiles) built on top
of it.

## `--shared-lib` mode

```sh
$ ebc mylib.bas --shared-lib -o mylib
```

Like `--lib`, `mylib.bas` may only contain declarations at the top level -
no executable statement, and (mutually exclusive) not combinable with
`--lib` itself. Unlike `--lib`'s static archive, this produces a real,
dynamically loadable shared library another program can `dlopen()` (or
`LoadLibrary` on Windows) at runtime - the gap this mode fills is a
library meant for a consumer that isn't itself an eBasic/`ebc`-built
program (a plugin host, a scripting-language binding, an OS add-on
mechanism such as Haiku's own `BScreenSaver` add-ons).

By default, an ordinary top-level `SUB`/`FUNCTION` still gets its usual
mangled internal name (`eb_<name>`, wrapped in real C++, not a stable ABI
symbol) - exactly like every other `ebc` build. Only a `SUB`/`FUNCTION`
written with a **real body** inside an `Extern "C" ... End Extern` block
opts in to becoming a real, stable, unmangled export another program can
find by name:

```basic
Extern "C"
    Function AddNumbers(a AS INTEGER, b AS INTEGER) AS INTEGER
        AddNumbers = a + b
    End Function
End Extern
```

This reuses the same `Extern "C" ... End Extern` syntax already used for
[importing](../reference/extern-interop.md) a real C function - here, a
real, bodied definition inside the block marks it as an *export* instead.
Restrictions:

- Only `Extern "C"` (not `Extern "C++"`) may contain a bodied definition -
  a mangled C++-linkage "export" isn't a stable ABI boundary.
- Only a free (non-member) `SUB`/`FUNCTION` can be exported - not a
  `TYPE` method.
- Every parameter and the return type must be C-ABI-compatible - `STRING`
  is rejected (same restriction as an ordinary `Extern`/`Declare`
  signature); use `ZSTRING`/`ZSTRING PTR` instead. Nothing about the rest
  of the file is restricted - an internal helper (not itself exported)
  can freely use `STRING`, `TYPE`s with constructors, or anything else,
  since only the opted-in export crosses the C-ABI boundary.

Produces, per platform:

| Platform | Shared library | Import library |
| --- | --- | --- |
| Linux / Haiku | `lib<output>.so` | - |
| macOS | `lib<output>.dylib` | - |
| Windows (MinGW) | `<output>.dll` | `lib<output>.dll.a` |

plus the same `<output>.iface.bas`/`<output>.libs` sidecar files `--lib`
produces - a shared library can *also* still be depended on by another
eBasic package via its ordinary mangled symbols, not just its opt-in C
exports (the two mechanisms are orthogonal).

This is exactly the mechanism [`ebpm`](ebpm.md) uses under the hood for a
package's `[shared-lib]` target.

## See also

- [Getting Started](getting-started.md)
- [`ebpm`](ebpm.md)
- [`EXTERN` / C-C++ Interop](../reference/extern-interop.md)
