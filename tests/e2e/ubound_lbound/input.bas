' eBasic's UBound/LBound array-bounds introspection (see
' docs/reference/procedures-and-arrays.md#ubound--lbound) - a compiler
' special form, not an ordinary pre-declared function, since arrays are
' plain std::vector<T> with no generic Extern "C++" binding possible.
' Covers a static array with the default 0 lower bound, a static array
' with an explicit lo TO hi bound, a dynamic array before and after
' REDIM (including the "empty array's UBound is LBound - 1" edge case),
' the idiomatic FOR i = LBound(arr) TO UBound(arr) loop, and a local
' array inside a FUNCTION (its own separate scope's arrayLowerBoundVar_
' entry).

DIM fixed(5) AS INTEGER
PRINT LBound(fixed)
PRINT UBound(fixed)

DIM ranged(3 TO 7) AS INTEGER
PRINT LBound(ranged)
PRINT UBound(ranged)

DIM dyn() AS INTEGER
PRINT LBound(dyn)
PRINT UBound(dyn)

REDIM dyn(4)
PRINT LBound(dyn)
PRINT UBound(dyn)

REDIM dyn(2 TO 9)
PRINT LBound(dyn)
PRINT UBound(dyn)

DIM arr(3 TO 7) AS INTEGER
DIM i AS INTEGER
FOR i = LBound(arr) TO UBound(arr)
    arr(i) = i * 10
NEXT i

DIM sum AS INTEGER
sum = 0
FOR i = LBound(arr) TO UBound(arr)
    sum = sum + arr(i)
NEXT i
PRINT sum

FUNCTION SumLocal() AS INTEGER
    DIM local(1 TO 4) AS INTEGER
    DIM j AS INTEGER
    FOR j = LBound(local) TO UBound(local)
        local(j) = j
    NEXT j
    DIM total AS INTEGER
    total = 0
    FOR j = LBound(local) TO UBound(local)
        total = total + local(j)
    NEXT j
    SumLocal = total
END FUNCTION

PRINT SumLocal()

PRINT "ubound/lbound ok"
