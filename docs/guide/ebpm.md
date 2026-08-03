# `ebpm` - the package manager

```
usage: ebpm <command> [args]
       ebpm [-v | --version] [-h | --help]
commands:
  new <name> [--lib]   scaffold a new package directory <name>/
  init [--lib]         scaffold a package in the current directory
  build                build the package in the current directory
  run [-- args...]     build (if stale), then run the [bin] target
  test                 build+run every tests/*.bas as a standalone
                       program; pass = exit code 0
  add <name> [--version <req>]
                       look up <name> in the package index and add it
                       to [dependencies] (highest version, or the
                       highest satisfying <req> if given)
  remove <name>        remove <name> from [dependencies]
  list                 show the resolved dependency tree
  search <term>        search the package index by name/description
  update [<name>]      re-resolve a registry dependency (or all of
                       them) to its current highest satisfying
                       version, ignoring any existing lockfile pin
```

Cargo-style package management: a TOML manifest (`ebasic.toml`), path/git/
registry dependencies, a lockfile for reproducible builds. Every command
operates on the package rooted at the current directory.

## `ebpm new <name>` / `ebpm init`

```sh
$ ebpm new hello_pkg
Created binary package 'hello_pkg' at hello_pkg
```

Scaffolds a package directory (`new`) or turns the current directory into
one (`init`) - an `ebasic.toml` manifest plus a stub `src/main.bas`
(`PRINT "Hello from <name>!"`). Add `--lib` to scaffold a library package
instead (a stub `src/lib.bas` with a placeholder `FUNCTION`, and a `[lib]`
manifest section instead of `[bin]`).

## The manifest: `ebasic.toml`

```toml
[package]
name = "hello_pkg"
version = "0.1.0"

[bin]
name = "hello_pkg"
path = "src/main.bas"
```

- **`[package]`** - `name` (required), `version` (metadata only - not
  constraint-solved against anything yet).
- **`[bin]`** - present iff the package builds an executable. `name`
  defaults to `[package].name`, `path` to `"src/main.bas"`, if omitted.
- **`[lib]`** - present iff the package builds a library. `path` defaults to
  `"src/lib.bas"` if omitted. A package may have `[bin]`, `[lib]`, or both.
- **`[dependencies]`** - see below.

```toml
[package]
name = "myapp"
version = "0.1.0"

[bin]
name = "myapp"
path = "src/main.bas"

[dependencies]
mathlib = { path = "../mathlib" }
```

### Dependencies

Each entry under `[dependencies]` names **exactly one** of `path`, `git`, or
`version`:

```toml
[dependencies]
# a path dependency - a sibling/local directory
mathlib = { path = "../mathlib" }

# a git dependency - cloned/fetched into a shared, global cache
# (~/.ebpm/cache/git/) the first time, then reused
gitlib = { git = "https://example.com/gitlib.git", branch = "master" }

# a registry dependency - looked up by name in the package index (see
# "Registry dependencies" below); the bare-string shorthand implies `version`
mathlib-demo = "^1.0"
```

A git dependency may additionally name **at most one** of `branch`, `tag`,
or `rev` (defaults to the repository's own default branch if none is
given). A registry dependency's `version` requirement may not be combined
with `branch`/`tag`/`rev` - the index entry it resolves to already names its
own.

### Platform-specific dependencies

```toml
[dependencies]
common_lib = { path = "../common_lib" }        # built on every platform

[target.windows.dependencies]
win32_bindings = { path = "../win32_bindings" }

[target.linux.dependencies]
gtk4_bindings = { path = "../gtk4_bindings" }

[target.macos.dependencies]
cocoa_bindings = { path = "../cocoa_bindings" }

[target.haiku.dependencies]
haiku_bindings = { path = "../haiku_bindings" }
```

A `[target.<os>.dependencies]` section (`<os>` is one of `windows`, `linux`,
`macos`, or `haiku` - an unrecognized name is a build error, not a silent
no-op) is only built when `<os>` matches the platform `ebpm` itself is
running on - `ebpm` never cross-compiles, so "the current target" always
means "whatever OS this `ebpm` binary is". Combined with an unconditional
`[dependencies]` entry (built everywhere) and each `.bas` file's own
[auto-defined platform macros](../reference/preprocessor.md#platform-macros)
to conditionally `#include` the right interface, this is how a program
calls into a genuinely different platform API per OS - e.g. Win32 on
Windows, GTK4 on Linux:

```basic
#ifdef __FB_WIN32__
    #include "win32_bindings.iface.bas"
#endif
#ifdef __FB_LINUX__
    #include "gtk4_bindings.iface.bas"
#endif
```

### Registry dependencies

Cargo-style: instead of hand-writing a git URL, name a package and a
version requirement, and `ebpm` looks it up in a central **package index**
(a git repository - see [Configuring the index](#configuring-the-index)
below) that maps a name to real published versions, each backed by a real
git URL/tag:

```toml
[dependencies]
# either of these is equivalent - the bare string is shorthand for the table form
mathlib-demo = "^1.0"
# mathlib-demo = { version = "^1.0" }
```

A version requirement is one of three forms (Cargo's own syntax):

| Form | Example | Matches |
|---|---|---|
| Caret (default) | `^1.2.3`, `1.2.3` | `>=1.2.3, <2.0.0` |
| Tilde | `~1.2.3` | `>=1.2.3, <1.3.0` |
| Exact | `=1.2.3` | exactly `1.2.3` |

A caret requirement's upper bound tracks the first nonzero component -
`^0.2.3` means `>=0.2.3, <0.3.0`, and `^0.0.3` means `>=0.0.3, <0.0.4` -
matching npm/Cargo's own well-known `0.x` handling, since a `0.x` release is
conventionally allowed to break compatibility on every minor (or, below
`0.0`, every patch) bump. A partial version (`^1`, `~1.2`) is allowed;
`=1.2.3` requires the full triple.

Resolution is **non-backtracking**: a package name resolves to exactly one
version for the whole dependency graph. If two dependencies (direct or
transitive) name the same registry package with genuinely incompatible
requirements, that's a hard build error naming both requirers - not an
attempt to keep two versions side by side.

#### `ebpm add` / `ebpm remove`

```sh
$ ebpm add mathlib-demo
Added mathlib-demo v1.1.0 to [dependencies]

$ ebpm add mathlib-demo --version "~1.0.0"
Added mathlib-demo v1.0.0 to [dependencies]

$ ebpm remove mathlib-demo
Removed mathlib-demo from [dependencies]
```

`add` looks up the name in the index and picks its highest published
version - or the highest satisfying `--version <req>`, if given - then adds
`name = "^<version>"` to `[dependencies]`. `remove` deletes that entry.
Both edit `ebasic.toml` at the text level (preserving your own formatting/
comments elsewhere in the file), so a hand-written multi-line dependency
entry is left untouched with an error rather than risked - edit those by
hand instead.

#### `ebpm list` / `ebpm search`

```sh
$ ebpm list
mathlib-demo v1.1.0 (registry, commit ac1ba0e87a52)
gitlib (git, commit 9f2c1a0b3d4e)
mathlib (path: /home/you/hello_pkg/../mathlib)

$ ebpm search math
mathlib-demo - A tiny demo math library for ebpm - proves the whole add/version-resolution pipeline end to end.
```

`list` shows the *current project's* fully resolved dependency tree (one
line per dependency, annotated with how it was reached). `search <term>`
instead browses the *index itself* - every published package whose name or
description contains `<term>` - regardless of whether your project depends
on it.

#### `ebpm update`

```sh
$ ebpm update mathlib-demo
Updating mathlib-demo v1.0.0 -> v1.1.0

$ ebpm update
mathlib-demo is already up to date (v1.1.0)
```

An ordinary `build`/`run`/`test` never re-consults the index once a
registry dependency is pinned in `ebasic.lock` (see below) - `update` is the
explicit, deliberate way to pick up a newer compatible release: it
re-resolves the named dependency (or every registry dependency, with no
name given), ignoring its existing pin, and rewrites the lockfile.

#### Configuring the index

A registry dependency's package index is itself just a git repository - one
`<name>.toml` file per package (the exact same shape `ebpm`'s own
`ebasic.toml` uses). Its URL is resolved in priority order:

1. The `EBASIC_INDEX_URL` environment variable.
2. `~/.ebpm/config.toml`'s `[registry] index = "..."`.
3. A hardcoded default - `ebpm`'s own starter index
   (`https://github.com/yann64/ebpm-index`), seeded with a small demo
   package (`mathlib-demo`) that exercises the whole `add`/version-
   resolution pipeline end to end.

`EBASIC_INDEX_URL` (env var, not the real Cargo-equivalent `CARGO_HOME`
naming) deliberately follows this project's own `EBASIC_LIBRARY_PATH`
precedent (see [`ebpm build`](#ebpm-build) below) rather than any
third-party tool's variable name - useful for pointing at a private/
internal index, or a local index during index-authoring/testing, without
touching `~/.ebpm/config.toml`.

### The lockfile: `ebasic.lock`

Auto-generated/updated by `ebpm build` (and `run`/`test`, which build first
if stale) - records every dependency's resolved directory, and, for a git
dependency, the exact commit it resolved to:

```toml
# Auto-generated by ebpm - do not edit by hand.

[[package]]
name = "mathlib"
path = "/home/you/hello_pkg/../mathlib"

[[package]]
name = "mathlib-demo"
path = "/home/you/.ebpm/cache/git/.../mathlib-demo@v1.1.0"
commit = "ac1ba0e87a52..."
version = "1.1.0"
git = "https://github.com/yann64/mathlib-demo"
ref = "v1.1.0"
```

A git dependency's pinned commit is checked out again on a repeat build,
even if the remote branch has since moved - this is what makes a repeat
build reproducible. A registry dependency's entry additionally records the
resolved `version`, `git` URL, and `ref` directly - a registry dependency
has no URL in the manifest at all (only a name + requirement), so without
this a pinned rebuild would need to consult the index again just to find
one; with it, an ordinary rebuild is fully offline-capable, exactly like a
plain git dependency's own pinned rebuild. Don't edit `ebasic.lock` by hand;
commit it to version control so collaborators get the same resolved
dependencies.

## `ebpm build`

```sh
$ ebpm build
   Compiling mathlib (lib)
   Compiling myapp (bin)
```

Builds the package's own target(s) plus its entire dependency graph, in
dependency order - each package receives the `-I`/`-L`/`-l` flags for its
*entire transitive* dependency closure, not just its direct dependencies
(so a chain like `app -> mid -> base` still links `base` into `app`'s final
binary even though `app`'s own source never `#include`s `base`'s interface
directly).

`ebpm` has no `-cxx` flag of its own - it invokes `ebc` as a child process,
which inherits the `CXX` environment variable directly, so `CXX=clang++
ebpm build` (or `run`/`test`) selects `clang++` as the backend compiler for
every package in the graph, exactly like passing `-cxx clang++` to `ebc`
directly (see [`ebc`](ebc.md)) - verified with a real multi-package build.

A package's manifest has no field for a real, external (non-`ebpm`) system
library's own search directory - e.g. wherever a `Lib "gtk-4"` binding's
actual `libgtk-4` lives. `EBASIC_LIBRARY_PATH` (a `:`-separated list of
directories, matching the shape of `PATH`/`LIBRARY_PATH` itself) fills this
gap: every directory it names is forwarded as `-L` to every `ebc` invocation
across the whole build. Deliberately not the real `LIBRARY_PATH` - `ebc`/g++
would pick that up too, but on Haiku that exact name is also consulted by
the OS's own runtime_loader, and pointing it at a directory that doesn't
hold every shared object the OS itself needs breaks process startup
entirely (not just `ebc`'s) - `EBASIC_LIBRARY_PATH` avoids that collision on
every platform.

## `ebpm run`

```sh
$ ebpm run
   Compiling hello_pkg (bin)
Hello from hello_pkg!
```

Builds (only if stale - a simple mtime check against the resolved
dependency graph's sources) then runs the `[bin]` target. Arguments after a
literal `--` are forwarded to the built program verbatim:

```sh
$ ebpm run -- --some-flag
```

## `ebpm test`

```sh
$ ebpm test
   Compiling squarelib (lib)
ok
test square_test ... ok

test result: ok. 1 passed; 0 failed
```

Builds the package, then compiles and runs every `tests/*.bas` file as a
standalone program that `#include`s the package's own auto-generated
interface (exactly like an external consumer would) - a test passes if its
program exits `0`.

## See also

- [Getting Started](getting-started.md)
- [`ebc`](ebc.md) - `ebpm` uses `ebc --lib` mode under the hood for `[lib]` targets
