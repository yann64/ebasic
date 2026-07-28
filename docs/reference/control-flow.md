# Control Flow

## `:` (statement separator)

A colon separates multiple statements on a single line - anywhere a newline
would otherwise be required:

```basic
DIM x AS INTEGER
DIM y AS INTEGER
x = 1 : y = 2
PRINT x     ' 1
PRINT y     ' 2
```

(A colon immediately after a bare identifier at the start of a line is
instead parsed as a [label](#goto-and-labels), e.g. `skip:`.)

## `PRINT`

```
PRINT expr, expr, ...
PRINT
```

Writes each comma-separated argument in order, with **no separator
inserted between them** - unlike some BASIC dialects, a comma does not
print a space, tab, or column-align. Exactly one trailing newline is
written after all arguments (or immediately, for a bare `PRINT` with no
arguments):

```basic
PRINT "a", "b", 1, 2   ' ab12
PRINT                  ' (just a newline)
```

## `IF` / `THEN` / `ELSEIF` / `ELSE` / `END IF`

```
IF condition THEN
    ...
ELSEIF condition THEN
    ...
ELSE
    ...
END IF
```

`ELSEIF`/`ELSE` are optional; any number of `ELSEIF` branches are allowed.
There is no single-line `IF condition THEN statement` form - every `IF`
requires `END IF`.

```basic
DIM n AS INTEGER
n = 7
IF n > 10 THEN
    PRINT "big"
ELSEIF n > 5 THEN
    PRINT "medium"
ELSE
    PRINT "small"
END IF
' prints: medium
```

## `SELECT CASE`

```
SELECT CASE selector
CASE value1, value2, ...
    ...
CASE valueN
    ...
CASE ELSE
    ...
END SELECT
```

Each `CASE` line may list multiple comma-separated values (matches if the
selector equals *any* of them). `CASE ELSE`, if present, must be the last
`CASE` in the block - a Sema error otherwise. `CASE value1 TO value2` and
`CASE IS <op> value` range forms are not supported yet.

A `CASE` value's type only needs to be *loosely* compatible with the
selector's - any two numeric types are comparable (looser than assignment's
own type-compatibility rule), and `STRING` only matches `STRING`.

```basic
DIM n AS INTEGER
n = 7
SELECT CASE n
CASE 1, 2, 3
    PRINT "low"
CASE 7
    PRINT "seven"
CASE ELSE
    PRINT "other"
END SELECT
' prints: seven
```

## `FOR` / `TO` / `STEP` / `NEXT`

```
FOR var = start TO end [STEP increment]
    ...
NEXT var
```

`STEP` defaults to `1` if omitted. A negative `STEP` counts down (`end`
must be less than `start` for the loop to run at all):

```basic
DIM i AS INTEGER
DIM total AS INTEGER
FOR i = 1 TO 10 STEP 2
    total = total + i
NEXT i
PRINT total        ' 25 (1+3+5+7+9)

FOR i = 5 TO 1 STEP -1
    PRINT i         ' 5, 4, 3, 2, 1
NEXT i
```

## `DO` / `LOOP`, `WHILE` / `WEND`

`DO`'s test can go before the loop body (`DO WHILE`/`DO UNTIL`, checked
before each iteration including the first) or after it (`LOOP
WHILE`/`LOOP UNTIL`, checked after each iteration - so the body always runs
at least once), and can be phrased as `WHILE` (loop while the condition is
true) or `UNTIL` (loop until it becomes true, i.e. while it's false) - all
four combinations are independently valid:

```
DO WHILE condition   /  DO UNTIL condition     (pre-test)
    ...
LOOP

DO
    ...
LOOP WHILE condition  /  LOOP UNTIL condition   (post-test)

WHILE condition         ' pre-test, alternate spelling of DO WHILE...LOOP
    ...
WEND
```

```basic
DIM i AS INTEGER
i = 0
DO WHILE i < 3
    PRINT i         ' 0, 1, 2
    i = i + 1
LOOP

i = 0
DO UNTIL i >= 3
    PRINT i         ' 0, 1, 2
    i = i + 1
LOOP

i = 0
DO
    PRINT i         ' 0, 1  (runs at least once, tests after)
    i = i + 1
LOOP UNTIL i >= 2
```

## `EXIT`

```
EXIT FOR
EXIT DO
EXIT WHILE
EXIT SUB
EXIT FUNCTION
```

Exits the nearest enclosing construct of the **matching kind specifically**
- `EXIT FOR` from inside a `DO` loop nested in a `FOR` loop exits the `FOR`,
not the `DO`, even though the `DO` is the innermost construct:

```basic
DIM i AS INTEGER
FOR i = 1 TO 5
    DO
        IF i = 3 THEN
            EXIT FOR    ' exits the FOR, not just this DO
        END IF
        PRINT i
        EXIT DO         ' exits this DO after one iteration
    LOOP
NEXT i
```

`EXIT SUB`/`EXIT FUNCTION` are covered together with
[`SUB`/`FUNCTION`](procedures-and-arrays.md#sub-function) themselves.

## `GOTO` and labels

```
GOTO label
...
label:
```

A label is a bare identifier followed by `:` on its own line. `GOTO`
transfers control unconditionally to it - anywhere in the same procedure
(or at top level).

```basic
GOTO skip
PRINT "should not print"
skip:
PRINT "after goto"
```

## `GOSUB` / `RETURN`

```
GOSUB label
...
label:
    ...
    RETURN
```

Calls the code starting at `label` as a subroutine - execution continues
there until `RETURN`, which resumes right after the `GOSUB` that was called
(each call site gets its own correct return point, including nested
`GOSUB`s calling other `GOSUB` targets). This is distinct from `GOTO`, which
never returns.

```basic
DIM counter AS INTEGER
counter = 0

GOSUB Bump
PRINT counter          ' 1
GOSUB Bump
PRINT counter          ' 2

GOTO SkipSub
Bump:
    counter = counter + 1
    RETURN
SkipSub:
```

## See also

- [Types and Literals](types-and-literals.md)
- [Operators](operators.md)
- [Procedures and Arrays](procedures-and-arrays.md)
