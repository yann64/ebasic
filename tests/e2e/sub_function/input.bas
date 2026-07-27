' Recursive FUNCTION (both RETURN and the FuncName = value return form)
FUNCTION Factorial(n AS INTEGER) AS INTEGER
    IF n <= 1 THEN
        RETURN 1
    END IF
    Factorial = n * Factorial(n - 1)
END FUNCTION

PRINT Factorial(5)

' Mutual recursion: IsOdd forward-references IsEven, defined later
FUNCTION IsOdd(n AS INTEGER) AS BOOLEAN
    IF n = 0 THEN
        RETURN FALSE
    END IF
    RETURN IsEven(n - 1)
END FUNCTION

FUNCTION IsEven(n AS INTEGER) AS BOOLEAN
    IF n = 0 THEN
        RETURN TRUE
    END IF
    RETURN IsOdd(n - 1)
END FUNCTION

PRINT IsOdd(7)
PRINT IsEven(7)

' BYVAL vs BYREF
SUB Increment(BYREF x AS INTEGER)
    x = x + 1
END SUB

SUB TryIncrement(BYVAL x AS INTEGER)
    x = x + 1
END SUB

DIM counter AS INTEGER
counter = 10
CALL Increment(counter)
PRINT counter
CALL TryIncrement(counter)
PRINT counter

' STRING parameters default to BYREF
SUB Shout(s AS STRING)
    s = s & "!"
END SUB

DIM msg AS STRING
msg = "hello"
CALL Shout(msg)
PRINT msg

' A local DIM shadows a global of the same name inside the procedure only
DIM shadowed AS INTEGER
shadowed = 100

SUB ShadowTest()
    DIM shadowed AS INTEGER
    shadowed = 999
    PRINT shadowed
END SUB

CALL ShadowTest()
PRINT shadowed

CALL Increment(counter)
PRINT counter

' EXIT FUNCTION
FUNCTION FirstPositive(a AS INTEGER, b AS INTEGER) AS INTEGER
    IF a > 0 THEN
        FirstPositive = a
        EXIT FUNCTION
    END IF
    FirstPositive = b
END FUNCTION

PRINT FirstPositive(-1, 42)
PRINT FirstPositive(7, 42)
