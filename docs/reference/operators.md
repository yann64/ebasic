# Operators

All binary/unary operators eBasic recognizes, dictionary-style, followed by
a full precedence table.

## Arithmetic operators

### `+` (addition), `-` (subtraction), `*` (multiplication)

Standard arithmetic on any two numeric operands (integer-family or
float-family - see [Types and Literals](types-and-literals.md#primitive-types)).
Mixed integer/float operands promote to float; mixed integer widths/signedness
promote to the wider/unsigned type.

### `/` (real division)

Always produces a real (floating-point) result, even for two integer
operands - eBasic never silently truncates on `/`:

```basic
PRINT 7 / 2     ' 3.5
```

### `\` (integer division)

Truncating integer division - only meaningful (and only allowed) between
two non-float operands:

```basic
PRINT 7 \ 2     ' 3
```

### `MOD`

Integer remainder:

```basic
PRINT 7 MOD 2   ' 1
```

### `^` (power)

Exponentiation, always producing a real (`double`) result. **Right-associative**
(`2 ^ 2 ^ 3` is `2 ^ (2 ^ 3)`, i.e. `256`, not `(2 ^ 2) ^ 3`):

```basic
PRINT 2 ^ 3     ' 8
PRINT 2 ^ 2 ^ 3 ' 256
```

`^` binds tighter than unary `-`, so a leading minus applies to the whole
power expression: `-2 ^ 2` is `-(2 ^ 2)` = `-4`, not `(-2) ^ 2`. A minus
appearing *after* `^` binds to that operand directly: `2 ^ -1` is `2 ^ (-1)`
= `0.5`. See [Unary operators](#unary-operators).

## Bitwise / shift operators

### `SHL`, `SHR`

Bitwise left/right shift on integer-family operands:

```basic
PRINT 1 SHL 4   ' 16
PRINT 256 SHR 4 ' 16
```

## Logical / bitwise operators

### `AND`, `OR`, `XOR`, `NOT`

Bitwise on integer-family operands - and, because `BOOLEAN` is represented
as `-1`/`0` (all bits set, or none), these double as logical operators with
no separate logical-vs-bitwise distinction:

```basic
PRINT 5 AND 3        ' 1   (101 AND 011 = 001)
PRINT 5 OR 2         ' 7   (101 OR  010 = 111)
PRINT 5 XOR 1        ' 4   (101 XOR 001 = 100)
PRINT NOT 0          ' -1  (bitwise NOT)
PRINT NOT TRUE       ' 0   (NOT -1 = 0)
PRINT 3 > 2 AND 1 > 5 ' 0  (relational binds tighter than AND: (3>2) AND (1>5))
```

`NOT` is unary; `AND`/`OR`/`XOR` are binary.

## The `&` (string concatenation) operator

Concatenates two `STRING` operands. Unlike some BASIC dialects, `&` does
**not** implicitly convert a numeric operand to a string - both sides must
already be `STRING` (a Sema-enforced restriction, not just an unusual style
choice):

```basic
PRINT "abc" & "def"   ' abcdef
```

## Comparison operators

`=`, `<>`, `<`, `>`, `<=`, `>=` - compare two numeric operands, or two
`STRING` operands (lexicographic). Each produces a `BOOLEAN` (`-1`/`0`):

```basic
PRINT 3 > 2           ' -1
PRINT "abc" < "abd"   ' -1
PRINT "abc" = "abc"   ' -1
```

`SELECT CASE` uses a slightly looser compatibility rule than assignment for
comparing a `CASE` value against the selector - see
[Control Flow](control-flow.md#select-case).

## Unary operators

### `-` (negate)

Arithmetic negation. Binds looser than `^` but tighter than `*`/`/`/`\` -
see [`^`](#power) above for how this interacts with exponentiation.

### `NOT`

See [Logical / bitwise operators](#logical-bitwise-operators) above. Binds
looser than `AND` but tighter than a relational comparison, so `NOT a > b`
is `NOT (a > b)`.

### `@` (address-of), `*` (dereference)

Pointer operators - covered in
[Namespaces, Pointers, and Unions](namespaces-pointers-unions.md#pointers-ptr--)
since they only make sense together with the `PTR` type.

## Precedence table

From **tightest-binding** (evaluated first) to **loosest-binding**
(evaluated last):

| Level | Operators | Associativity |
|---|---|---|
| 1 (tightest) | `@`, `*` (address-of / deref, prefix) | - |
| 2 | `^` | right |
| 3 | unary `-` | - |
| 4 | `*`, `/` | left |
| 5 | `\` | left |
| 6 | `MOD` | left |
| 7 | `SHL`, `SHR` | left |
| 8 | `+`, `-` | left |
| 9 | `&` | left |
| 10 | `=`, `<>`, `<`, `>`, `<=`, `>=` | left |
| 11 | `NOT` (prefix) | - |
| 12 | `AND` | left |
| 13 | `OR` | left |
| 14 (loosest) | `XOR` | left |

Use parentheses freely to override precedence - `(2 + 3) * 4` always means
what it looks like.

## See also

- [Types and Literals](types-and-literals.md)
- [Control Flow](control-flow.md)
