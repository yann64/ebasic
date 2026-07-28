' A small, hand-written sample library, documented with ''' doc comments -
' this stands in for a real eBasic-authored standard library, which
' doesn't exist yet (see docs/architecture/roadmap.md's M7 notes). It
' exists purely to give docgen something real to run against.

''' A 2D point with integer coordinates.
TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

''' The ratio of a circle's circumference to its diameter, to a few
''' decimal places.
CONST Pi AS DOUBLE = 3.14159

''' Squares a number.
''' Returns n multiplied by itself.
FUNCTION Square(n AS INTEGER) AS INTEGER
    Square = n * n
END FUNCTION

' This one is intentionally left undocumented, to show docgen's
' "(undocumented)" fallback in the generated output.
FUNCTION Cube(n AS INTEGER) AS INTEGER
    Cube = n * n * n
END FUNCTION

''' Adds two points together, component-wise.
FUNCTION AddPoints(a AS Point, b AS Point) AS Point
    DIM result AS Point
    result.x = a.x + b.x
    result.y = a.y + b.y
    AddPoints = result
END FUNCTION
