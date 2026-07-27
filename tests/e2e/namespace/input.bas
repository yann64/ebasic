NAMESPACE MathUtils
    CONST PI AS SINGLE = 3.5
    DIM callCount AS INTEGER

    FUNCTION Square(n AS INTEGER) AS INTEGER
        callCount = callCount + 1
        Square = n * n
    END FUNCTION

    SUB Reset()
        callCount = 0
    END SUB
END NAMESPACE

' Qualified access from outside
PRINT MathUtils.PI
PRINT MathUtils.Square(5)
PRINT MathUtils.Square(6)
PRINT MathUtils.callCount

CALL MathUtils.Reset()
PRINT MathUtils.callCount

' A second, unrelated namespace with a same-named member - proves no collision
NAMESPACE StringUtils
    DIM callCount AS INTEGER

    FUNCTION Shout(s AS STRING) AS STRING
        callCount = callCount + 1
        Shout = s & "!"
    END FUNCTION
END NAMESPACE

DIM greeting AS STRING
greeting = "hi"
PRINT StringUtils.Shout(greeting)
PRINT StringUtils.callCount
PRINT MathUtils.callCount  ' unaffected by StringUtils' own callCount

' Reopening a namespace (adding more members in a second block)
NAMESPACE MathUtils
    FUNCTION Cube(n AS INTEGER) AS INTEGER
        Cube = n * n * n
    END FUNCTION
END NAMESPACE

PRINT MathUtils.Cube(3)

' A global variable with the same name as something inside a namespace -
' does not collide (different qualified keys)
DIM callCount AS INTEGER
callCount = 999
PRINT callCount
PRINT MathUtils.callCount
