' See docs/reference/type-oop.md

TYPE Shape
    name AS STRING
    Declare Constructor()
    Declare Virtual Function Area() AS SINGLE
End TYPE

Constructor Shape()
    name = "Shape"
End Constructor

Virtual Function Shape.Area() AS SINGLE
    Area = 0
End Function

TYPE Circle EXTENDS Shape
    radius AS SINGLE
    Declare Constructor()
    Declare Virtual Function Area() AS SINGLE Override
End TYPE

Constructor Circle()
    name = "Circle"
    radius = 2
End Constructor

Virtual Function Circle.Area() AS SINGLE
    Area = 3.14159 * radius * radius
End Function

Sub PrintArea(BYREF s AS Shape)
    PRINT s.name
    PRINT s.Area()
End Sub

DIM c AS Circle
CALL PrintArea(c)   ' dynamic dispatch: prints Circle's own Area, not Shape's

TYPE Vector2
    x AS SINGLE
    y AS SINGLE
END TYPE

Operator +(ByRef a AS Vector2, ByRef b AS Vector2) AS Vector2
    DIM r AS Vector2
    r.x = a.x + b.x
    r.y = a.y + b.y
    Return r
End Operator

DIM v1 AS Vector2
DIM v2 AS Vector2
v1.x = 1 : v1.y = 2
v2.x = 3 : v2.y = 4

DIM total AS Vector2
total = v1 + v2
PRINT total.x
PRINT total.y
