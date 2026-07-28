# Preprocessor

A line-based textual pass that runs before lexing: object-like macros,
conditional compilation, and file inclusion. **Macro names and directive
keywords are case-sensitive** - the one place eBasic departs from the rest
of the (otherwise case-insensitive) language.

## `#define`

```
#define NAME value
```

Defines an object-like macro (no parameters) - every later occurrence of
`NAME` is replaced with `value` (skipping over string-literal contents and
anything after a `'` comment on the same line). Case-sensitive: `GREETING`
and `greeting` are different macros.

```basic
#define GREETING "hello"
#define MAX 10
PRINT GREETING   ' hello
PRINT MAX        ' 10
```

## `#ifdef` / `#ifndef` / `#else` / `#endif`

```
#ifdef NAME
    ...
#endif

#ifndef NAME
    ...
#else
    ...
#endif
```

Includes or excludes a block of source depending on whether `NAME` has been
`#define`d. There is no `#elif` - chain nested `#ifdef`/`#else` blocks for a
multi-way choice. Excluded text is blanked, not removed, so line numbers
(and therefore diagnostics) from the remaining code stay accurate.

```basic
#define MAX 10
#ifdef MAX
PRINT "MAX is defined"        ' this prints
#endif
#ifndef NOTDEFINED
PRINT "NOTDEFINED is not defined"   ' this prints
#else
PRINT "should not print"
#endif
```

See [Platform Macros](#platform-macros) below for the one macro set that's
`#define`d automatically, without any `#define` in your own source.

## `#include` / `#include once`

```
#include "path"
#include once "path"
```

Splices another file's content in place, recursively (with circular-include
detection). `path` is resolved relative to the *including* file's own
directory first; if that fails, each `-I <dir>` given to `ebc` (see the
[`ebc` guide](../guide/ebc.md)) is tried in order as a fallback - this is
what lets a package `#include` a dependency's auto-generated interface file
without knowing its exact relative filesystem path. There's no enforced
file extension - `.bi` ("BASIC Include") is the conventional choice, matching
FreeBASIC, but not required.

`#include once` skips re-including a file already brought in (directly or
transitively) at least once - `#include` (without `once`) always splices
again. Both forms count as "already included" for each other's purposes.

```basic
' mathutils.bi:
#include once "constants.bi"
FUNCTION Square(n AS INTEGER) AS INTEGER
    Square = n * n
END FUNCTION

' constants.bi:
CONST GREETING AS STRING = "hello from constants.bi"

' input.bas:
#include "mathutils.bi"
#include once "constants.bi"   ' already included via mathutils.bi - skipped
PRINT GREETING                  ' hello from constants.bi
PRINT Square(6)                 ' 36
```

## Platform macros

Exactly one of the following is `#define`d automatically, before your own
source runs - never more than one, never zero. Matches real FreeBASIC's own
macro names where FreeBASIC has a matching target (maximizing source
portability with real FreeBASIC code); `__FB_HAIKU__` is eBasic's own
extension, since Haiku isn't a real FreeBASIC target.

| Macro | Defined when built on |
|---|---|
| `__FB_WIN32__` | Windows |
| `__FB_LINUX__` | Linux |
| `__FB_DARWIN__` | macOS |
| `__FB_HAIKU__` | Haiku |

```basic
#ifdef __FB_WIN32__
    Extern "C" Lib "user32"
        ' Win32 declarations
    End Extern
#endif
#ifdef __FB_LINUX__
    Extern "C" Lib "gtk-4"
        ' GTK4 declarations
    End Extern
#endif
```

This is the source-level half of platform-specific code - see
[`ebpm`'s target-specific dependencies](../guide/ebpm.md#platform-specific-dependencies)
for the package-manifest half (choosing a different *dependency* per
platform).

## See also

- [`ebpm`: Platform-specific dependencies](../guide/ebpm.md#platform-specific-dependencies)
- [Control Flow](control-flow.md)
