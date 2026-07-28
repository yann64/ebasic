# Procedures and Arrays

## `SUB` / `FUNCTION`

```
SUB Name(params...)
    ...
END SUB

FUNCTION Name(params...) AS ReturnType
    ...
END FUNCTION
```

A `FUNCTION` returns a value two ways: `RETURN expr` (returns immediately),
or assigning to the function's own name (`FuncName = expr`) - execution
continues after such an assignment (it's just an ordinary-looking
assignment, not an implicit return), so the last one to execute before the
function ends is what's actually returned.

```basic
FUNCTION Factorial(n AS INTEGER) AS INTEGER
    IF n <= 1 THEN
        RETURN 1
    END IF
    Factorial = n * Factorial(n - 1)   ' the FuncName = value form
END FUNCTION

PRINT Factorial(5)   ' 120
```

`SUB`/`FUNCTION` may call each other regardless of declaration order,
including mutual recursion - every signature is registered before any body
is checked.

A `SUB` is called with `CALL Name(args)`; a bare `Name(args)` (no `CALL`) is
not a valid statement. A `FUNCTION` is called as an expression (`x =
Factorial(5)`, `PRINT Factorial(5)`).

### Parameters: `BYVAL` / `BYREF`

```
SUB Name(BYVAL x AS Type)   ' pass by value - a copy, mutations don't escape
SUB Name(BYREF x AS Type)   ' pass by reference - mutations are visible to the caller
```

If neither is written explicitly, the default depends on the parameter's
type: `BYREF` for `STRING` and any user-defined `TYPE`, `BYVAL` for every
other built-in type (matching FreeBASIC's own default).

```basic
SUB Increment(BYREF x AS INTEGER)
    x = x + 1
END SUB

SUB TryIncrement(BYVAL x AS INTEGER)
    x = x + 1   ' mutates only the local copy
END SUB

DIM counter AS INTEGER
counter = 10
CALL Increment(counter)
PRINT counter        ' 11
CALL TryIncrement(counter)
PRINT counter        ' still 11
```

### `EXIT SUB` / `EXIT FUNCTION`

Returns immediately from the enclosing `SUB`/`FUNCTION` (a `FUNCTION`
returns whatever was last assigned to its own name before the `EXIT`):

```basic
FUNCTION FirstPositive(a AS INTEGER, b AS INTEGER) AS INTEGER
    IF a > 0 THEN
        FirstPositive = a
        EXIT FUNCTION
    END IF
    FirstPositive = b
END FUNCTION

PRINT FirstPositive(-1, 42)   ' 42
PRINT FirstPositive(7, 42)    ' 7
```

## Scoping

A variable `DIM`'d inside a `SUB`/`FUNCTION` is local to it, and shadows a
global of the same name for the duration of that procedure only:

```basic
DIM shadowed AS INTEGER
shadowed = 100

SUB ShadowTest()
    DIM shadowed AS INTEGER
    shadowed = 999
    PRINT shadowed     ' 999 (the local)
END SUB

CALL ShadowTest()
PRINT shadowed          ' 100 (the global, unaffected)
```

## Static arrays

See [`DIM`](types-and-literals.md#dim) for static array declaration syntax
(`DIM arr(n) AS Type` / `DIM arr(lo TO hi) AS Type`) - a static array's size
is fixed at declaration and cannot be changed with `REDIM`.

## `REDIM` / `REDIM PRESERVE`

```
REDIM name(newUpperBound) [AS Type]
REDIM PRESERVE name(newUpperBound) [AS Type]
```

Resizes a **dynamic** array - one originally declared with empty parens
(`DIM name() AS Type`). Redimensioning a fixed-size array (declared with an
explicit bound) is a Sema error: `'<name>' is a fixed-size array and cannot
be REDIM'd`.

Plain `REDIM` discards the array's previous contents (a fresh, zeroed
array). `REDIM PRESERVE` keeps existing elements - growing adds
zero-initialized new slots at the end; shrinking simply drops the
now-out-of-range tail.

```basic
DIM arr() AS INTEGER
REDIM arr(4)
DIM i AS INTEGER
FOR i = 0 TO 4
    arr(i) = i * i
NEXT i
' arr is now 0, 1, 4, 9, 16

REDIM PRESERVE arr(7)   ' grow: arr(0..4) unchanged, arr(5..7) zeroed
FOR i = 5 TO 7
    arr(i) = 100 + i
NEXT i

REDIM PRESERVE arr(2)   ' shrink: arr(0..2) unchanged, arr(3..7) dropped

REDIM arr(2)            ' plain REDIM: arr(0..2) is now freshly zeroed
```

A dynamic array with an explicit lower bound also works with `REDIM`:

```basic
DIM names() AS STRING
REDIM names(1 TO 3)
names(1) = "one"
```

## See also

- [Types and Literals](types-and-literals.md) - `DIM`, static arrays
- [Control Flow](control-flow.md)
- [TYPE and Object-Oriented Programming](type-oop.md) - methods (which follow the same `BYVAL`/`BYREF`/parameter rules as free `SUB`/`FUNCTION`)
