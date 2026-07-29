' A two-level EXTENDS chain, the last link adding no fields of its own
' (the exact shape a wrapper-library TYPE hierarchy tends to have - see
' the eb-gtk4 project that found this gap) - proves a derived, plain-data
' TYPE (no methods/ctor/dtor anywhere in its own chain) is exported across
' a --lib boundary correctly, base(s) included.

TYPE Shape
    shapeId AS INTEGER
END TYPE

TYPE Colored EXTENDS Shape
    color AS INTEGER
END TYPE

TYPE Circle EXTENDS Colored
END TYPE

FUNCTION MakeCircle(id AS INTEGER, color AS INTEGER) AS Circle
    DIM c AS Circle
    c.shapeId = id
    c.color = color
    MakeCircle = c
END FUNCTION

FUNCTION CircleId(c AS Circle) AS INTEGER
    CircleId = c.shapeId
END FUNCTION

FUNCTION CircleColor(c AS Circle) AS INTEGER
    CircleColor = c.color
END FUNCTION
