' Default parameter values (a trailing `= <literal>` in a parameter list) -
' BYVAL-only, trailing-only, literal-only (see docs/reference/
' procedures-and-arrays.md). Exercises a free FUNCTION, a free SUB, and a
' TYPE method, each called both with and without the defaulted argument.

FUNCTION Greet(BYVAL name AS STRING, BYVAL greeting AS STRING = "Hello") AS STRING
    Greet = greeting & ", " & name & "!"
END FUNCTION

FUNCTION AddWithDefault(a AS INTEGER, b AS INTEGER = 10) AS INTEGER
    AddWithDefault = a + b
END FUNCTION

SUB PrintTimes(BYVAL message AS STRING, times AS INTEGER = 1)
    DIM i AS INTEGER
    FOR i = 1 TO times
        PRINT message
    NEXT i
END SUB

TYPE Counter
    value AS INTEGER
    Declare Function AddTo(amount AS INTEGER = -1) AS INTEGER
END TYPE

FUNCTION Counter.AddTo(amount AS INTEGER = -1) AS INTEGER
    THIS.value = THIS.value + amount
    AddTo = THIS.value
END FUNCTION

PRINT Greet("World")
PRINT Greet("World", "Hi")

PRINT AddWithDefault(5)
PRINT AddWithDefault(5, 20)

CALL PrintTimes("once")
CALL PrintTimes("twice", 2)

DIM c AS Counter
c.value = 10
PRINT c.AddTo()
PRINT c.AddTo(5)
