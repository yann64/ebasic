# Math Library

eBasic's standard math, numeric-conversion, and base-conversion
procedures - `ABS`/`SGN`/`SQR`/trig/`CInt`-family/`HEX$`-family, matching
FreeBASIC's own set. Like the [String Library](string-library.md) and
[File Library](file-library.md), these aren't reserved keywords - they're
ordinary functions, always pre-declared (via the same hidden
`Extern "C++"` block the compiler splices into every program before your
own source), callable from any `.bas` file with no `#include` needed:

```basic
PRINT Abs(-3.5)     ' 3.5
PRINT Sqr(9)         ' 3
PRINT Hex(255)       ' FF
```

Because they're pre-declared this way, a program can't declare its own
`SUB`/`FUNCTION`/variable under any of these exact names (a real
"already declared" error) - see the String Library's own note on this.

## Trigonometric and exponential functions

```
Abs(n AS DOUBLE) AS DOUBLE
Sgn(n AS DOUBLE) AS INTEGER
Sqr(n AS DOUBLE) AS DOUBLE
Sin(n AS DOUBLE) AS DOUBLE
Cos(n AS DOUBLE) AS DOUBLE
Tan(n AS DOUBLE) AS DOUBLE
Asin(n AS DOUBLE) AS DOUBLE
Acos(n AS DOUBLE) AS DOUBLE
Atn(n AS DOUBLE) AS DOUBLE
Atan2(y AS DOUBLE, x AS DOUBLE) AS DOUBLE
Exp(n AS DOUBLE) AS DOUBLE
Log(n AS DOUBLE) AS DOUBLE
```

`Sgn` returns `-1`, `0`, or `1`. All angles are in radians, matching
real FreeBASIC. `Log` is the natural logarithm (base *e*).

## `Int` / `Fix`

```
Int(n AS DOUBLE) AS DOUBLE
Fix(n AS DOUBLE) AS DOUBLE
```

Both round toward an integer, but differently for negative numbers:
`Int` floors toward negative infinity (`Int(-1.5) = -2`), `Fix`
truncates toward zero (`Fix(-1.5) = -1`) - matching real FreeBASIC's own
distinction between the two.

## `Rnd` / `Randomize`

```
Rnd(n AS DOUBLE = 1) AS DOUBLE
Randomize(seed AS DOUBLE = 0)
```

`Rnd` returns the next random value in `[0, 1)`. `Randomize` reseeds the
shared random number generator - `Randomize(0)` (the default, i.e. a
bare `CALL Randomize()`) seeds from the current time; a nonzero seed
reseeds deterministically, so the same seed always produces the same
sequence of `Rnd` calls afterward:

```basic
CALL Randomize(42)
PRINT Rnd(1)    ' always the same value, since the seed is fixed
```

Real FreeBASIC's own `Rnd(n)` has extra legacy quirks for `n < 0`
(deterministic reseed) and `n = 0` (repeat the last value) - **not
replicated here**, a deliberate simplification; `n` is accepted (for
call-site compatibility with FreeBASIC source) but otherwise unused.

## Numeric conversions (`Cxxx`)

```
CByte(n AS DOUBLE) AS BYTE
CUByte(n AS DOUBLE) AS UBYTE
CShort(n AS DOUBLE) AS SHORT
CUShort(n AS DOUBLE) AS USHORT
CInt(n AS DOUBLE) AS INTEGER
CUInt(n AS DOUBLE) AS UINTEGER
CLngInt(n AS DOUBLE) AS LONGINT
CULngInt(n AS DOUBLE) AS ULONGINT
CSng(n AS DOUBLE) AS SINGLE
CDbl(n AS DOUBLE) AS DOUBLE
CBool(n AS DOUBLE) AS BOOLEAN
```

One conversion per distinct eBasic primitive type - not FreeBASIC's full
historical list, which has several names for what are, in eBasic, the
exact same underlying type (`INTEGER` and `LONG` are both a 32-bit
signed integer here, so there's no separate `CInt` vs. `CLng`).

Every conversion truncates toward zero (a plain `static_cast`) -
**unlike real FreeBASIC's own `CInt`/`CLng`, which round to the nearest
integer**, these deliberately match `Fix`'s truncating behavior instead,
for one simple, uniform rule across every `Cxxx` function. A value
outside the target type's range wraps according to ordinary C++
narrowing-conversion rules (e.g. `CByte(200.9)` is `-56`), matching how
any other narrowing assignment in this language already behaves.

`CBool` follows this language's own `TRUE = -1` convention: any nonzero
input converts to `-1`, not `1`.

## Base conversions

```
Hex(n AS LONGINT) AS STRING
Oct(n AS LONGINT) AS STRING
Bin(n AS LONGINT) AS STRING
```

Format `n` as an unsigned hexadecimal/octal/binary string with no
prefix, sign, or leading zeros (`Hex(0)` is `"0"`, not `""`).
`Hex`/`Oct`/`Bin` are FreeBASIC's own `Hex$`/`Oct$`/`Bin$`, with the `$`
dropped to match this project's own `Len`/`Mid`/... naming convention.
Negative numbers are formatted as their full two's-complement bit
pattern in the target width (`Hex(-1)` is `"FFFFFFFFFFFFFFFF"`, all 64
bits of a `LONGINT`), not a minus sign.

## See also

- [String Library](string-library.md)
- [File Library](file-library.md)
