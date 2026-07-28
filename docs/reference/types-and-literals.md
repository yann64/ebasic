# Types and Literals

eBasic identifiers and keywords are **case-insensitive** (`Dim`, `DIM`, and
`dim` are the same keyword; `X` and `x` are the same variable).

## `DIM`

```
DIM name AS Type
DIM name(upperBound) AS Type
DIM name(lowerBound TO upperBound) AS Type
DIM name() AS Type
```

Declares a variable. Every `DIM` requires an explicit `AS Type` - there is no
type inference for variables (unlike `CONST`, below).

- `DIM name(upperBound) AS Type` declares a **static array** indexed `0` to
  `upperBound` inclusive (so `DIM scores(4) AS INTEGER` has 5 elements,
  `scores(0)` through `scores(4)`).
- `DIM name(lowerBound TO upperBound) AS Type` declares explicit bounds
  (`DIM grid(1 TO 3) AS INTEGER` has 3 elements, `grid(1)` through `grid(3)`).
- `DIM name() AS Type` (empty parens) declares a **dynamic array** - size 0
  until resized with `REDIM` (see [`REDIM`](procedures-and-arrays.md#redim--redim-preserve)).
  A dynamic array cannot be `REDIM`'d if it wasn't declared this way.

```basic
DIM age AS INTEGER
age = 30
PRINT age                 ' 30

DIM scores(4) AS INTEGER  ' indices 0..4
scores(0) = 10
scores(4) = 50

DIM grid(1 TO 3) AS INTEGER  ' indices 1..3
grid(1) = 1
```

## Primitive types

| Type | C++ representation | Notes |
|---|---|---|
| `BYTE` | `std::int8_t` | 8-bit signed |
| `UBYTE` | `std::uint8_t` | 8-bit unsigned |
| `SHORT` | `std::int16_t` | 16-bit signed |
| `USHORT` | `std::uint16_t` | 16-bit unsigned |
| `INTEGER` | `std::int32_t` | 32-bit signed |
| `LONG` | `std::int32_t` | 32-bit signed - same width as `INTEGER` |
| `UINTEGER` | `std::uint32_t` | 32-bit unsigned |
| `LONGINT` | `std::int64_t` | 64-bit signed |
| `ULONGINT` | `std::uint64_t` | 64-bit unsigned |
| `SINGLE` | `float` | 32-bit floating point |
| `DOUBLE` | `double` | 64-bit floating point |
| `BOOLEAN` | `std::int8_t` | see below |
| `STRING` | `::ebasic::rt::BString` | dynamic, always-copy value type |
| `ZSTRING` | `const char*` | C-compatible; meaningful mainly at an [`EXTERN`](extern-interop.md) boundary |

`BOOLEAN` is stored as `-1` (true) / `0` (false), not `1`/`0` - this is
BASIC's own convention (not C++'s), and it's why bitwise `AND`/`OR`/`XOR`/`NOT`
double as logical operators: see
[Operators](operators.md#logical--bitwise-operators).

`STRING` is a dynamic, always-copy value type (no reference counting/COW yet
- deferred until real string-heavy programs need it). Two `STRING`s can be
compared and concatenated - see [Operators](operators.md#the--string-concatenation-operator).

`ZSTRING` is a raw, null-terminated C string. A `STRING` converts to/from a
`ZSTRING`/`ZSTRING PTR` argument automatically at a call site (no manual
marshaling needed) - see [`EXTERN`](extern-interop.md).

There is no separate character type - a one-character `STRING` is used
instead.

## Literals

- **Integer literals**: a plain run of digits (`42`, `0`, `1000000`). No
  hex/octal/binary literal syntax, and no digit-grouping separators.
- **Double literals**: digits, a `.`, then more digits (`1.5`, `0.5`) - a
  bare `.5` or a trailing `5.` is not recognized as a double literal (the
  lexer requires at least one digit on each side of the `.`). No scientific
  ("e"-suffixed) notation.
- **Negative numbers** are not a distinct literal form - `-5` is the unary
  negate operator applied to the literal `5` (see
  [Operators](operators.md#unary-operators)).
- **String literals**: double-quoted (`"like this"`). A literal double quote
  inside a string is written doubled (`"she said ""hi"""`), the classic
  BASIC escaping convention - there is no backslash-escape syntax (`\n`,
  `\t`, ... are not special). A string literal cannot span multiple lines.
- **Boolean literals**: `TRUE` (`-1`) and `FALSE` (`0`).

```basic
PRINT 42          ' 42
PRINT 1.5         ' 1.5
PRINT "hello"     ' hello
PRINT TRUE        ' -1
PRINT FALSE       ' 0
```

## `CONST`

```
CONST name = expr
CONST name AS Type = expr
```

Declares a compile-time constant. Unlike `DIM`, the type is **inferred**
from `expr` when `AS Type` is omitted. `expr` must be a *structural* constant
expression - literals and references to already-declared `CONST`/`ENUM`
members, combined with unary/binary operators; eBasic does not evaluate the
expression at compile time (except for `ENUM` auto-increment, below) - the
generated C++ re-evaluates it, so any operation valid in a `CONST`
initializer is exactly whatever the backend C++ compiler accepts there.

```basic
CONST MAX_USERS = 100
PRINT MAX_USERS          ' 100

CONST GREETING AS STRING = "hello"
PRINT GREETING           ' hello
```

## `ENUM`

```
ENUM Name
    Member1
    Member2 = expr
    ...
END ENUM
```

Declares a set of named integer constants. A member with no explicit value
is the previous member's value **+ 1** (or `0` for the first member); an
explicit `= expr` resumes auto-increment from that value for the next
member. `ENUM` has no `AS Type` - members are always integer-valued.

```basic
ENUM Direction
    North      ' 0
    South      ' 1
    East       ' 2
    West       ' 3
END ENUM
PRINT North     ' 0
PRINT West      ' 3
```

An enumerator with an explicit value, followed by auto-increment resuming
from it:

```basic
ENUM Color
    Red              ' 0
    Green            ' 1
    Blue             ' 2
    Purple = 10      ' 10 (explicit)
    Pink             ' 11 (10 + 1)
END ENUM
```

## See also

- [Operators](operators.md)
- [Control Flow](control-flow.md)
- [Procedures and Arrays](procedures-and-arrays.md) - `REDIM`/`REDIM PRESERVE` for dynamic arrays
- [`EXTERN` / C-C++ Interop](extern-interop.md) - `ZSTRING`'s real use case
