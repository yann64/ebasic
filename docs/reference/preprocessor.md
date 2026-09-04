# Preprocessor

A line-based textual pass that runs before lexing: macros, conditional
compilation, and file inclusion - closely matching real FreeBASIC's own
preprocessor (see [Deviations from FreeBASIC](#deviations-from-freebasic)
for the handful of places it intentionally doesn't). **Macro names and
directive keywords are case-sensitive** - the one place eBasic departs from
the rest of the (otherwise case-insensitive) language.

## `#define`

```
#define NAME value
#define NAME(params) body
#define NAME(params, rest...) body
```

The first form defines an object-like macro - every later occurrence of
`NAME` is replaced with `value` (skipping over string-literal contents and
anything after a `'` comment on the same line). Case-sensitive: `GREETING`
and `greeting` are different macros. Expansion is recursive - a macro's
body may itself reference other macros - but a macro is never re-expanded
inside its own expansion (so `#define X X` doesn't loop forever).

```basic
#define GREETING "hello"
#define MAX 10
PRINT GREETING   ' hello
PRINT MAX        ' 10
```

The second and third forms define a **function-like** macro: `(` must
follow `NAME` immediately, with no space, or the whole thing is parsed as
an object-like macro whose value happens to start with `(`. Parameters are
substituted textually wherever they appear in `body`; a trailing
`name...` parameter collects every argument beyond the fixed ones,
rejoined with `, ` (so a macro body can re-split it itself, e.g. with
`InStr`). A call may have fewer arguments than declared parameters -
missing ones are treated as empty text, not an error - but more arguments
than a non-variadic macro declares is rejected.

```basic
#define MyMul(a, b) ((a) * (b))
PRINT MyMul(6, 7)   ' 42
```

Two operators are available inside a macro body:

- **`#param`** (stringize) turns the argument's own source text into a
  string literal - `#define SEE(x) PRINT #x, " = ", x` then `SEE(count)`
  prints `count = <value>`.
- **`lhs##rhs`** (concatenate) pastes two adjacent pieces of text into one
  token, which can then form a real identifier - `#define Concat(t, n)
  t##n` then `Concat(hello, world)` expands to the single identifier
  `helloworld`.

## `#macro` / `#endmacro`

```
#macro NAME(params)
    ' one or more statements, or even directives
#endmacro
```

The multi-line form of a function-like `#define`. Unlike `#define`, a
`#macro`'s body may itself contain real directives (`#if`/`#else`/`#endif`,
nested `#define`s, ...), evaluated at invocation time using that call's own
argument bindings - but as a consequence, **a `#macro` may only be invoked
as the entire content of a source line** (its own statement), never
embedded inside a larger expression the way a function-like `#define` can
be. A space between `NAME` and `(` is allowed here (unlike `#define`'s
header).

```basic
#macro test1(arg1, arg2...)
    PRINT arg1
    #if #arg2 = ""
        PRINT "2nd argument not passed"
    #else
        PRINT arg2
    #endif
#endmacro

test1("1", "2")   ' 1 \n 2
test1("3")        ' 3 \n 2nd argument not passed
```

## `#undef`

```
#undef NAME
```

Forgets a `#define`d or `#macro`'d name, as if it had never been defined.

## `#if` / `#elseif` / `#ifdef` / `#ifndef` / `#elseifdef` / `#elseifndef` / `#else` / `#endif`

```
#if (expr)
    ...
#elseif (expr)
    ...
#else
    ...
#endif

#ifdef NAME
    ...
#elseifdef OTHER_NAME
    ...
#elseifndef THIRD_NAME
    ...
#else
    ...
#endif
```

`#ifdef`/`#ifndef` test whether `NAME` has been `#define`d; `#if` evaluates
a small expression (integer arithmetic `+ - * / mod`, comparisons `= <> <
> <= >=`, `and`/`or`/`not`, parens, and `defined(NAME)`) and takes the
branch if it's non-zero. Exactly one branch of an `#if`/`#elseif`/.../
`#else` chain ever runs, even if a later condition would also be true -
just like an ordinary `If`/`ElseIf` chain. Excluded text is blanked, not
removed, so line numbers (and therefore diagnostics) from the remaining
code stay accurate.

```basic
#define DEBUG_LEVEL 2
#if (DEBUG_LEVEL >= 3)
PRINT "verbose"
#elseif (DEBUG_LEVEL >= 1)
PRINT "normal"        ' this prints
#else
PRINT "quiet"
#endif
```

String operands are also supported in `#if`/`#elseif`, but only for `=`
and `<>` - not ordering or arithmetic - which is enough for the common
`#if #someArg = ""` idiom shown under `#macro` above.

See [Platform Macros](#platform-macros) below for the one macro set that's
`#define`d automatically, without any `#define` in your own source.

## `#print` / `#error` / `#assert`

```
#print message
#error message
#assert (expr)
```

`#print` writes `message` (macro-expanded) to standard output while
compiling - purely informational, doesn't affect the build. `#error`
reports `message` as a compile error (via the same diagnostics every other
stage uses) and lets compilation continue scanning for further problems,
rather than aborting immediately - consistent with how every other eBasic
compiler error already accumulates before the build finally fails.
`#assert` evaluates `expr` the same way `#if` does and reports a compile
error (quoting `expr`'s own source text) if it's false.

## Predefined macros: `__LINE__` / `__FILE__` / `__DATE__` / `__TIME__`

Unlike the platform macros below, these vary per use site (or per compile)
rather than being fixed for the whole build, so there's nothing to
`#define` them to - they resolve dynamically wherever they're written.

| Macro | Value |
|---|---|
| `__LINE__` | The current line number, as an integer. |
| `__FILE__` | The current file's path, as a string. |
| `__DATE__` | The compilation date, as a `"mm-dd-yyyy"` string. |
| `__TIME__` | The compilation time, as a `"hh:mm:ss"` string. |

Inside a macro's expansion, `__LINE__`/`__FILE__` resolve to the call
site, not the macro's own definition - matching how a compile error
*inside* a macro expansion is also reported against the call site (see
[Deviations from FreeBASIC](#deviations-from-freebasic)).

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

## Deviations from FreeBASIC

eBasic's preprocessor closely follows FreeBASIC's own, but deliberately
stops short of full parity in a few places:

- **`defined()` only sees `#define`/`#macro` names, not `Const`/`Dim`d
  symbols.** Real FreeBASIC's preprocessor shares a symbol table with the
  compiler proper, so `defined()` can see any declared symbol; eBasic's
  preprocessor is architecturally a separate pass run entirely before the
  lexer even runs, so it has no visibility into `Const`/`Dim` at all. In
  practice this only matters for `defined()` - `#ifdef`/`#ifndef` were
  always `#define`-only.
- **No `#lang`, `#cmdline`, `#pragma`, `#line`, `#inclib`, or `#libpath`.**
  These control FreeBASIC's own dialect switching, backend/assembler
  selection, and linker model, none of which have an equivalent in
  eBasic's g++-backend pipeline. To link against a native library, list it
  in `[lib.needs]`/`-l` instead (see the [`ebc` guide](../guide/ebc.md)).
- **No `__FB_ARG_COUNT__`/`__FB_EVAL__`/`__FB_IIF__`/`__FB_JOIN__`/
  `__FB_QUERY_SYMBOL__`/`__FB_QUOTE__`/`__FB_UNIQUEID__`-family
  meta-macros, and no `__FB_VERSION__`/`__FB_OPTION_*__`/asm-and-backend-
  selector intrinsic defines.** These describe FreeBASIC-compiler-internal
  concepts (its own token-manipulation macro language, its asm dialect,
  its GAS/GCC backend switch, per-file `OPTION` state) with no eBasic
  equivalent. The platform macros above and `__LINE__`/`__FILE__`/
  `__DATE__`/`__TIME__` cover the common, portable subset.
- **`#if`/`#elseif` string comparisons support only `=` and `<>`**, not
  ordering (`<`, `>`, ...) or arithmetic - enough for the stringize-and-
  compare-to-`""` idiom shown under `#macro` above, without a full string
  type in the expression evaluator.
- **A `#macro` invocation's diagnostics (and `__LINE__`/`__FILE__` inside
  its body) are attributed to the call site, not a precise line inside the
  macro's own definition.** The same is true of any macro expansion in
  general - this matches how compilers commonly simplify macro-expansion
  diagnostics, at the cost of a less precise location for an error that
  originates deep inside a `#macro` body.
- **A macro call may pass fewer arguments than declared parameters**
  without it being an error (missing ones become empty text) - slightly
  more lenient than FreeBASIC, chosen to keep the implementation simple
  rather than modeling FreeBASIC's own exact arity rules.

## See also

- [`ebpm`: Platform-specific dependencies](../guide/ebpm.md#platform-specific-dependencies)
- [Control Flow](control-flow.md)
