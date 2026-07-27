' Basic field read/write
TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

DIM p AS Point
p.x = 3
p.y = 4
PRINT p.x
PRINT p.y

' Whole-struct assignment (default memberwise copy)
DIM q AS Point
q = p
q.x = 100
PRINT q.x
PRINT q.y
PRINT p.x  ' unaffected by q's mutation - confirms it was a real copy, not aliasing

' Nested record
TYPE Line
    startPt AS Point
    endPt AS Point
END TYPE

DIM ln AS Line
ln.startPt.x = 1
ln.startPt.y = 2
ln.endPt.x = 10
ln.endPt.y = 20
PRINT ln.startPt.x
PRINT ln.endPt.y

' Array of records
DIM pts(2) AS Point
DIM i AS INTEGER
FOR i = 0 TO 2
    pts(i).x = i * 10
    pts(i).y = i * 100
NEXT i
FOR i = 0 TO 2
    PRINT pts(i).x
    PRINT pts(i).y
NEXT i

' Record with a STRING field
TYPE Person
    name AS STRING
    age AS INTEGER
END TYPE

DIM somebody AS Person
somebody.name = "Alice"
somebody.age = 30
PRINT somebody.name
PRINT somebody.age

' Record as a SUB/FUNCTION parameter (defaults to BYREF, verified against FB docs)
SUB MovePoint(pt AS Point, dx AS INTEGER, dy AS INTEGER)
    pt.x = pt.x + dx
    pt.y = pt.y + dy
END SUB

FUNCTION Distance2(a AS Point, b AS Point) AS INTEGER
    DIM dx AS INTEGER
    DIM dy AS INTEGER
    dx = a.x - b.x
    dy = a.y - b.y
    Distance2 = dx * dx + dy * dy
END FUNCTION

DIM origin AS Point
origin.x = 0
origin.y = 0
CALL MovePoint(origin, 5, 7)
PRINT origin.x  ' mutated - confirms BYREF default for UDT parameters
PRINT origin.y

DIM target AS Point
target.x = 3
target.y = 4
PRINT Distance2(origin, target)
