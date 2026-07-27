DIM arr() AS INTEGER
DIM i AS INTEGER

REDIM arr(4)
FOR i = 0 TO 4
    arr(i) = i * i
NEXT i
FOR i = 0 TO 4
    PRINT arr(i)
NEXT i

' Grow with PRESERVE: old data should survive
REDIM PRESERVE arr(7)
FOR i = 5 TO 7
    arr(i) = 100 + i
NEXT i
FOR i = 0 TO 7
    PRINT arr(i)
NEXT i

' Shrink with PRESERVE
REDIM PRESERVE arr(2)
FOR i = 0 TO 2
    PRINT arr(i)
NEXT i

' Plain REDIM (no PRESERVE): old data discarded, fresh zeroed array
REDIM arr(2)
FOR i = 0 TO 2
    PRINT arr(i)
NEXT i

' Dynamic array with an explicit lower bound
DIM names() AS STRING
REDIM names(1 TO 3)
names(1) = "one"
names(2) = "two"
names(3) = "three"
PRINT names(1)
PRINT names(2)
PRINT names(3)

' A local dynamic array inside a SUB
SUB Fill(BYVAL n AS INTEGER)
    DIM local() AS INTEGER
    REDIM local(n)
    DIM j AS INTEGER
    FOR j = 0 TO n
        local(j) = j + 1
    NEXT j
    PRINT local(n)
END SUB

CALL Fill(3)
