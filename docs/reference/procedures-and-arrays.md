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

A `FUNCTION` is called as an expression (`x = Factorial(5)`, `PRINT
Factorial(5)`). A `SUB` has no return value, so it's called with its own
statement form instead - see `CALL` below.

## `CALL`

```
CALL Name(args...)
```

Calls a `SUB` (or a `FUNCTION`, discarding its return value) as a
statement. A bare `Name(args)` with no `CALL` is **not** a valid statement -
`CALL` is required:

```basic
SUB Greet()
    PRINT "hi"
END SUB

CALL Greet()
```

Inside a [method](type-oop.md#methods), `CALL This.Method(args)` calls
another method on the same instance, and `CALL Base.Method(args)` calls the
immediate base's own implementation directly, bypassing any `Override` -
see [`EXTENDS`](type-oop.md#extends---inheritance).

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

### Default parameter values

```basic
FUNCTION Greet(BYVAL name AS STRING, BYVAL greeting AS STRING = "Hello") AS STRING
    Greet = greeting & ", " & name & "!"
END FUNCTION

PRINT Greet("World")          ' Hello, World!
PRINT Greet("World", "Hi")    ' Hi, World!
```

A trailing `= <literal>` makes a parameter optional - a call may omit it
(and every parameter after it, which must also have a default) and get the
literal's value instead. Three rules, all enforced at compile time:

- **Trailing-only**: once one parameter in a list has a default, every
  parameter after it must too.
- **`BYVAL`-only**: a `BYREF` parameter can't have a default (there's no
  addressable temporary for a missing argument to bind to).
- **Literal-only**: the default must be a literal (a number, string,
  boolean, or a negated numeric literal like `-1`) - not an arbitrary
  expression (so it can't reference another parameter, a global, etc.).

Applies to a free `SUB`/`FUNCTION`, an `Extern`/`Declare` signature, and a
`TYPE` method alike:

```basic
TYPE Counter
    value AS INTEGER
    Declare Function AddTo(amount AS INTEGER = -1) AS INTEGER
END TYPE

FUNCTION Counter.AddTo(amount AS INTEGER = -1) AS INTEGER
    THIS.value = THIS.value + amount
    AddTo = THIS.value
END FUNCTION

DIM c AS Counter
c.value = 10
PRINT c.AddTo()     ' 9  (defaults to -1)
PRINT c.AddTo(5)    ' 14
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

## Generics (`OF`)

A free (non-method) `SUB`/`FUNCTION` may declare a single type parameter
with an `(OF T)` clause right after its own name, before the regular
parameter list:

```basic
FUNCTION Max(OF T) (a AS T, b AS T) AS T
    IF a > b THEN
        Max = a
    ELSE
        Max = b
    END IF
END FUNCTION

PRINT Max(1, 2)         ' 2 - T = INTEGER
PRINT Max(1.5, 2.5)     ' 2.5 - T = DOUBLE, a separate instantiation
```

`T` is usable as an ordinary type name anywhere in the parameter list,
return type, and body. There's no separate "instantiate" step to write -
the concrete type is inferred from whichever argument is declared with
`T`'s own type at the call site (`Max(1, 2)` infers `T = INTEGER` from
either argument), and a fresh, fully type-checked copy of the
declaration is compiled for each distinct concrete type actually used,
deduplicated automatically (calling `Max` a second time with two more
`INTEGER`s reuses the same compiled copy, not a new one). A type that
can't be inferred (no parameter is declared `AS T`) or that needs more
arguments than were given to see it is a compile error, not a runtime
one:

```basic
FUNCTION MakeZero(OF T) () AS T
    MakeZero = 0
END FUNCTION

PRINT MakeZero()   ' error: cannot infer type parameter 'T' - no
                    ' parameter uses it directly
```

A `BYVAL`/`BYREF` parameter typed `AS T` follows the *concrete* type's
own default once instantiated (`BYVAL` for a numeric type, `BYREF` for
`STRING` or a `TYPE`) unless written explicitly - the same rule an
ordinary, non-generic parameter already follows:

```basic
TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

SUB BumpX(OF T) (p AS T)   ' T = Point here -> BYREF, mutates the caller's own value
    p.x = p.x + 100
END SUB

DIM pt AS Point
pt.x = 10
CALL BumpX(pt)
PRINT pt.x   ' 110
```

Not yet supported: a generic `TYPE` (only `SUB`/`FUNCTION` can take an
`(OF T)` clause today), more than one type parameter, and a generic
*method* (an `(OF T)` clause on a TYPE's own member procedure is a
compile error) - each a deliberate, documented scope cut for this first
slice, not an oversight.

## Coroutines (`Async`)

A `SUB`/`FUNCTION` marked `Async` compiles to a real C++20 coroutine.
A `FUNCTION` returns either `Task(OF T)` (one eventual value) or
`Generator(OF T)` (a lazily-pulled sequence); a `SUB` is implicitly a
valueless `Task` (no `AS` clause needed - a plain `SUB` never has one):

```basic
FUNCTION CountUp(n AS INTEGER) Async AS Generator(OF INTEGER)
    DIM i AS INTEGER
    FOR i = 1 TO n
        YIELD i
    NEXT
END FUNCTION

FUNCTION DoubleIt(x AS INTEGER) Async AS Task(OF INTEGER)
    RETURN x * 2
END FUNCTION
```

A `Generator(OF T)` is consumed with a pull-based `MoveNext()`/
`Current()` pair (eBasic has no `FOR EACH`/range concept yet to
consume it more directly) - `MoveNext()` advances to the next `YIELD`ed
value (or reports there isn't one), `Current()` reads it:

```basic
DIM gen AS Generator(OF INTEGER)
gen = CountUp(5)
DIM total AS INTEGER
DO WHILE gen.MoveNext()
    total = total + gen.Current()
LOOP
PRINT total   ' 15
```

`AWAIT expr` unwraps a `Task(OF T)`'s own value - but only from *inside
another* `Async` `SUB`/`FUNCTION`: real C++ forbids `co_await` in
`main()`, and top-level eBasic code compiles into `main()`, so `AWAIT`
at the top level (or inside an ordinary, non-`Async` procedure) is a
compile error, not a runtime one. From top-level code, read an
already-completed `Task`'s value with `.Result()` instead - this
runtime has no real scheduler/thread pool/async I/O yet (eBasic itself
has no concurrency primitives at all), so a `Task` always runs
synchronously, straight through to completion, by the time the call
that produced it returns:

```basic
FUNCTION AddThemUp() Async AS Task(OF INTEGER)
    DIM a AS INTEGER
    a = AWAIT DoubleIt(5)     ' legal: inside another Async FUNCTION
    RETURN a + (AWAIT DoubleIt(10))
END FUNCTION

DIM t AS Task(OF INTEGER)
t = AddThemUp()
PRINT t.Result()              ' 30 - AWAIT itself would be rejected here
```

Inside a `Generator`, `RETURN` never takes a value (`YIELD` produces
its values instead) - only a bare `RETURN`, to end the generator
early, is allowed. Inside a `Task`-shaped `Async` `FUNCTION`, the
`FuncName = value` self-return convention ordinary `FUNCTION`s use is
not supported - use `RETURN` explicitly.

Not yet supported: a real scheduler/thread pool/async I/O (every
coroutine here runs synchronously - `Async`/`Task`/`Generator` are a
syntax for suspendable control flow, not concurrency, this round);
`AWAIT`ing a `Generator` (pull it with `MoveNext`/`Current` instead);
a `TASK`/`GENERATOR` crossing an `EXTERN`/`DECLARE` boundary (no more
C-ABI-compatible than `STRING` is).

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

## `UBound` / `LBound`

```
UBound(name) AS INTEGER
LBound(name) AS INTEGER
```

The current highest/lowest valid index of array `name` - static or
dynamic, with or without an explicit lower bound, matching real
FreeBASIC. `LBound` is `0` unless the array was declared with an
explicit `lo TO hi` bound; `UBound` always reflects the array's
*current* size, so it tracks a dynamic array across `REDIM`:

```basic
DIM arr() AS INTEGER
PRINT LBound(arr)          ' 0
PRINT UBound(arr)          ' -1 - an empty array's UBound is LBound - 1

REDIM arr(4)
PRINT UBound(arr)          ' 4

DIM ranged(3 TO 7) AS INTEGER
PRINT LBound(ranged)       ' 3
PRINT UBound(ranged)       ' 7
```

The most common use is driving a loop without hardcoding the array's
size:

```basic
DIM i AS INTEGER
FOR i = LBound(arr) TO UBound(arr)
    arr(i) = i * i
NEXT i
```

Unlike every other function in eBasic's standard library, `UBound`/
`LBound` aren't ordinary pre-declared functions - they're a compiler
special form, resolved at compile time against the array's own
generated bookkeeping (there's no way to bind a single ordinary
function to "any array of any element type" the way `Len`/`Abs`/etc.
bind to a concrete C++ type). Because of this, the argument must be a
**bare array name**, not an arbitrary expression or a non-array
variable - `UBound(3 + 4)` and `UBound(someInt)` are both rejected at
compile time, not just at runtime.

## See also

- [Types and Literals](types-and-literals.md) - `DIM`, static arrays
- [Control Flow](control-flow.md)
- [TYPE and Object-Oriented Programming](type-oop.md) - methods (which follow the same `BYVAL`/`BYREF`/parameter rules as free `SUB`/`FUNCTION`)
