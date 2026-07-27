TYPE Shape
    name AS STRING

    Declare Constructor()
    Declare Virtual Function Area() AS SINGLE
    Declare Virtual Function Describe() AS STRING
END TYPE

Constructor Shape()
    name = "Shape"
End Constructor

Virtual Function Shape.Area() AS SINGLE
    Area = 0
End Function

Virtual Function Shape.Describe() AS STRING
    Describe = name
End Function

TYPE Circle EXTENDS Shape
    radius AS SINGLE

    Declare Constructor()
    Declare Virtual Function Area() AS SINGLE Override
    Declare Virtual Function Describe() AS STRING Override
END TYPE

Constructor Circle()
    name = "Circle"
    radius = 2
End Constructor

Virtual Function Circle.Area() AS SINGLE
    Area = 3.14159 * radius * radius
End Function

Virtual Function Circle.Describe() AS STRING
    ' Base.Method() bypasses the override, calling Shape's own version.
    Describe = Base.Describe() & " (circle)"
End Function

TYPE Square EXTENDS Shape
    side AS SINGLE

    Declare Constructor()
    Declare Virtual Function Area() AS SINGLE Override
END TYPE

Constructor Square()
    name = "Square"
    side = 4
End Constructor

Virtual Function Square.Area() AS SINGLE
    Area = side * side
End Function

' A BYREF Shape parameter, called with a Circle and a Square: virtual
' dispatch through the reference must call each one's own override, not
' Shape's plain version (which would happen if the value were sliced).
Sub PrintInfo(BYREF s AS Shape)
    PRINT s.Describe()
    PRINT s.Area()
End Sub

DIM c AS Circle
DIM sq AS Square

CALL PrintInfo(c)
CALL PrintInfo(sq)

' Square didn't override Describe - it inherits Shape's plain version.
PRINT sq.Describe()

' Direct (non-virtual, static-type) calls, for comparison.
PRINT c.Area()
PRINT c.name
PRINT c.radius
