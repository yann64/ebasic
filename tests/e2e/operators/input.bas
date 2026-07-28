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

Operator -(ByRef a AS Vector2, ByRef b AS Vector2) AS Vector2
    DIM r AS Vector2
    r.x = a.x - b.x
    r.y = a.y - b.y
    Return r
End Operator

Operator *(ByRef a AS Vector2, ByVal s AS SINGLE) AS Vector2
    DIM r AS Vector2
    r.x = a.x * s
    r.y = a.y * s
    Return r
End Operator

Operator =(ByRef a AS Vector2, ByRef b AS Vector2) AS BOOLEAN
    Return (a.x = b.x) AND (a.y = b.y)
End Operator

Operator <>(ByRef a AS Vector2, ByRef b AS Vector2) AS BOOLEAN
    Return NOT (a = b)
End Operator

DIM v1 AS Vector2
DIM v2 AS Vector2
DIM v3 AS Vector2

v1.x = 1 : v1.y = 2
v2.x = 3 : v2.y = 4
v3.x = 1 : v3.y = 1

DIM total AS Vector2
' Chained: (v1 + v2) is a temporary passed as the LHS operand of the
' second '+' - proves operator params bind to temporaries correctly.
total = v1 + v2 + v3
PRINT total.x
PRINT total.y

DIM diff AS Vector2
diff = v2 - v1
PRINT diff.x
PRINT diff.y

DIM factor AS SINGLE
factor = 2

DIM scaled AS Vector2
scaled = v1 * factor
PRINT scaled.x
PRINT scaled.y

PRINT (v1 = v1)
PRINT (v1 = v2)
PRINT (v1 <> v2)
PRINT (v1 <> v1)
