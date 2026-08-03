# String Library

eBasic's standard string manipulation procedures - `LEN`/`MID`/`LEFT`/
`RIGHT`/`INSTR`/etc., matching FreeBASIC's own set. Unlike everything else
in this reference, these aren't reserved keywords - they're ordinary
functions, always pre-declared (via a hidden `Extern "C++"` block the
compiler splices into every program before your own source), so they're
callable from any `.bas` file with no `#include` needed:

```basic
PRINT Len("hello")            ' 5
PRINT UCase("hello")          ' HELLO
PRINT Mid("hello world", 7, Len("hello world"))  ' world
```

Because they're pre-declared this way, a program can't declare its own
`SUB`/`FUNCTION`/variable under any of these exact names (a real
"already declared" error, the same as trying to redeclare any other
name already in scope) - in practice this matches how a real reserved
keyword behaves in most other languages' standard libraries.

All 1-based indexing (the first character of a string is position 1, not
0) and byte/ASCII-oriented (no Unicode awareness - `UCase`/`LCase`/
trimming operate one byte at a time). Every count/position argument is
*clamped* into a valid range rather than raising an error on out-of-range
input (an empty string, a count larger than the string, a not-found
search) - matching FreeBASIC's own forgiving behavior.

Several functions below take a default value for their trailing parameter
(see [Default parameter values](procedures-and-arrays.md#default-parameter-values))
in place of FreeBASIC's own optional-argument/overloaded forms, which
eBasic doesn't have.

## `Len`

```
Len(s AS STRING) AS INTEGER
```

The number of characters in `s`. `Len("")` is `0`.

## `Left` / `Right`

```
Left(s AS STRING, count AS INTEGER) AS STRING
Right(s AS STRING, count AS INTEGER) AS STRING
```

The leftmost/rightmost `count` characters of `s`. `count` is clamped into
`[0, Len(s)]` - a negative `count` gives `""`, one larger than `Len(s)`
gives the whole string.

```basic
PRINT Left("hello", 3)     ' hel
PRINT Right("hello", 3)    ' llo
PRINT Left("hi", 100)      ' hi
```

## `Mid`

```
Mid(s AS STRING, start AS INTEGER, length AS INTEGER = 2147483647) AS STRING
```

`length` characters of `s` starting at the 1-based position `start`.
`start` is clamped into `[1, Len(s) + 1]` (past-the-end gives `""`);
`length` is clamped to however many characters are actually available
from `start`. The default (a large sentinel) means "everything from
`start` to the end" - FreeBASIC's own 2-arg `MID$` form, reached here via
a default parameter value instead of true argument-count overloading
(which eBasic doesn't have).

```basic
PRINT Mid("hello world", 7, Len("hello world"))   ' world (to the end)
PRINT Mid("hello", 2, 3)                          ' ell
```

## `InStr` / `InStrRev`

```
InStr(haystack AS STRING, needle AS STRING, start AS INTEGER = 1) AS INTEGER
InStrRev(haystack AS STRING, needle AS STRING, start AS INTEGER = -1) AS INTEGER
```

The 1-based position of the first (`InStr`) or, searching backward
(`InStrRev`), last occurrence of `needle` in `haystack` - `0` if not
found. `InStr`'s `start` (default `1`) begins the search at or after that
1-based position; `InStrRev`'s `start` (default `-1`, meaning "from the
end") begins the backward search at or before it. FreeBASIC's own
`INSTR([start,] s1, s2)` puts the optional start position *first* - moved
to *last* here, since a default value must trail in eBasic.

```basic
PRINT InStr("hello world", "world")   ' 7
PRINT InStr("hello", "xyz")           ' 0 (not found)
PRINT InStrRev("abcabc", "abc")       ' 4
```

## `UCase` / `LCase`

```
UCase(s AS STRING) AS STRING
LCase(s AS STRING) AS STRING
```

`s` with every letter uppercased/lowercased (ASCII only).

## `LTrim` / `RTrim` / `Trim`

```
LTrim(s AS STRING, chars AS STRING = " ") AS STRING
RTrim(s AS STRING, chars AS STRING = " ") AS STRING
Trim(s AS STRING, chars AS STRING = " ") AS STRING
```

`s` with every leading (`LTrim`), trailing (`RTrim`), or both (`Trim`)
character found in `chars` removed - `chars` defaults to a plain space,
but any character set can be passed instead.

```basic
PRINT LTrim("  hi")       ' hi
PRINT RTrim("hi  ")       ' hi
PRINT Trim("  hi  ")      ' hi
```

## `Str`

```
Str(n AS DOUBLE) AS STRING
```

`n` formatted as text - a whole-number value never gets a trailing `.0`.
eBasic has no function overloading, so this single `DOUBLE`-typed `Str`
covers `INTEGER` arguments too via the language's own implicit
`INTEGER` -> `DOUBLE` widening.

```basic
PRINT Str(5)      ' 5
PRINT Str(3.14)   ' 3.14
```

## `Val`

```
Val(s AS STRING) AS DOUBLE
```

Parses `s`'s leading numeric text (skipping leading whitespace) - `0.0`
if nothing parses.

```basic
PRINT Val("42abc")   ' 42
PRINT Val("abc")     ' 0
```

## `Chr` / `Asc`

```
Chr(code AS INTEGER) AS STRING
Asc(s AS STRING) AS INTEGER
```

`Chr` returns a one-character string holding the given 8-bit character
code (masked into `[0, 255]`); `Asc` is the reverse - the character code
of `s`'s first character, `0` for an empty string.

```basic
PRINT Chr(65)          ' A
PRINT Asc("Ace")       ' 65
```

## `Space`

```
Space(n AS INTEGER) AS STRING
```

A string of `n` spaces (`n` clamped to `>= 0`).

## `Repeat`

```
Repeat(n AS INTEGER, s AS STRING) AS STRING
```

`s` repeated `n` times (`n` clamped to `>= 0`) - a small generalization of
FreeBASIC's `STRING$(n, char)` (which repeats a single character). Named
`Repeat` rather than `String` since `STRING` is already eBasic's reserved
type keyword.

```basic
PRINT Repeat(3, "ab")   ' ababab
```

## See also

- [Procedures and Arrays: Default parameter values](procedures-and-arrays.md#default-parameter-values)
- [`EXTERN` / C-C++ Interop](extern-interop.md) - the `Extern "C++"`
  mechanism these functions are implemented on top of internally
