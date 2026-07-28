# Namespaces, Pointers, and Unions

## `NAMESPACE`

```
NAMESPACE Name
    ...
END NAMESPACE
```

Groups `CONST`/`ENUM`/`DIM`/`SUB`/`FUNCTION` declarations under a qualified
name, accessed from outside as `Name.Member`. Only these declaration kinds
are allowed directly inside a `NAMESPACE` - nesting one `NAMESPACE` inside
another is not supported.

```basic
NAMESPACE MathUtils
    CONST PI AS SINGLE = 3.5

    FUNCTION Square(n AS INTEGER) AS INTEGER
        Square = n * n
    END FUNCTION
END NAMESPACE

PRINT MathUtils.PI          ' 3.5
PRINT MathUtils.Square(5)   ' 25
```

Code written *inside* a namespace sees its own members unqualified; from
outside, the `Name.` qualifier is required. A `NAMESPACE` can be **reopened**
- a second `NAMESPACE Name ... END NAMESPACE` block later in the same
program adds more members to the same namespace rather than creating a
conflicting second one:

```basic
NAMESPACE MathUtils
    FUNCTION Cube(n AS INTEGER) AS INTEGER
        Cube = n * n * n
    END FUNCTION
END NAMESPACE

PRINT MathUtils.Cube(3)   ' 27 - added to the same MathUtils from above
```

A global variable and a namespace member can share the same name without
colliding (they're different qualified keys internally), and two different
namespaces can each have their own member of the same name.

## Pointers (`PTR`, `@`, `*`, `->`)

```
DIM p AS Type PTR        ' a pointer to Type
DIM p AS Type PTR PTR    ' a pointer to a pointer to Type
DIM p AS ANY PTR         ' an untyped pointer (like C's void*)
```

- **`@expr`** (address-of) - takes the address of an lvalue (a variable,
  field, or array element - not a computed value or function-call result).
- **`*p`** (dereference) - accesses the value `p` points to. Can also be
  assigned through: `*p = value`.
- **`p->field`** - field access through a pointer; exactly equivalent to
  `(*p).field`.
- Pointer arithmetic (`p + n`, `p - n`) advances by `n` *elements* of the
  pointee type (like C), not raw bytes. Subtracting two pointers of the
  same type gives the distance between them, in elements.
- A pointer compares against `0` (there is no separate null-pointer
  keyword/literal).

```basic
TYPE Node
    value AS INTEGER
    nxt AS Node PTR
END TYPE

DIM n1 AS Node
DIM n2 AS Node
n1.value = 10
n2.value = 20
n1.nxt = @n2
n2.nxt = 0

DIM p AS Node PTR
p = @n1
DO WHILE p <> 0
    PRINT p->value    ' 10, then 20
    p = p->nxt
LOOP

' (*p).field is equivalent to p->field:
p = @n1
PRINT (*p).value      ' 10
```

Pointer arithmetic through an array:

```basic
DIM arr(4) AS INTEGER
arr(0) = 100
arr(1) = 200

DIM ip AS INTEGER PTR
ip = @arr(0)
PRINT *ip           ' 100
PRINT *(ip + 1)      ' 200
*ip = 999            ' deref-assignment
PRINT arr(0)         ' 999

DIM ip2 AS INTEGER PTR
ip2 = @arr(2)
PRINT ip2 - ip        ' 2 (element distance, not byte distance)
```

`ANY PTR` is the untyped/void-pointer-equivalent type - it can hold the
address of anything and compares against `0` the same way a typed pointer
does:

```basic
DIM anyP AS ANY PTR
anyP = @n1
PRINT anyP <> 0     ' -1
```

## `UNION`

```
UNION Name
    field1 AS Type1
    field2 AS Type2
    ...
END UNION
```

Structurally identical to `TYPE` (a name plus `field AS type` lines), except
every member **shares the same starting address** (classic C-style
type-punning), and a `UNION` may not contain, directly or nested, a
`STRING` field, nor any field with a constructor/destructor - both would be
memory-unsafe to alias. A `UNION`'s size is the size of its largest member,
not the sum of all of them.

```basic
TYPE Bytes4
    b0 AS BYTE
    b1 AS BYTE
    b2 AS BYTE
    b3 AS BYTE
END TYPE

UNION IntBytes
    asInt AS INTEGER
    bytes AS Bytes4
END UNION

DIM u AS IntBytes
u.asInt = 1
PRINT u.bytes.b0   ' 1 - the same memory viewed as 4 bytes
```

A `UNION` can embed a `TYPE` as a member (as above), and a `TYPE` can embed
a `UNION` as a field - either direction is allowed:

```basic
TYPE Tagged
    tag AS INTEGER
    data AS IntBytes
END TYPE
```

## See also

- [TYPE and Object-Oriented Programming](type-oop.md)
- [Procedures and Arrays](procedures-and-arrays.md)
