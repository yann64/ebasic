' See docs/reference/procedures-and-arrays.md

FUNCTION Factorial(n AS INTEGER) AS INTEGER
    IF n <= 1 THEN
        RETURN 1
    END IF
    Factorial = n * Factorial(n - 1)
END FUNCTION

PRINT Factorial(5)

SUB Increment(BYREF x AS INTEGER)
    x = x + 1
END SUB

DIM counter AS INTEGER
counter = 10
CALL Increment(counter)
PRINT counter

DIM arr() AS INTEGER
REDIM arr(4)
DIM i AS INTEGER
FOR i = 0 TO 4
    arr(i) = i * i
NEXT i

REDIM PRESERVE arr(7)
FOR i = 5 TO 7
    arr(i) = 100 + i
NEXT i
FOR i = 0 TO 7
    PRINT arr(i)
NEXT i
