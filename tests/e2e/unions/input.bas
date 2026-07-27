' Classic type-punning UNION: view the same 4 bytes as an INTEGER or as 4
' individually-addressable bytes via a nested TYPE (a union's members all
' share the SAME starting address - a sibling BYTE alone wouldn't give
' distinct byte offsets, they need to be sequential fields of a nested
' TYPE instead, which is the realistic way this idiom is written).
TYPE Bytes4
    b0 AS BYTE
    b1 AS BYTE
    b2 AS BYTE
    b3 AS BYTE
END TYPE

UNION IntBytes
    asInt AS INTEGER
    bytes AS Bytes4
END UNION

DIM u AS IntBytes
u.asInt = 0
PRINT u.bytes.b0
u.asInt = 1
PRINT u.bytes.b0
PRINT u.bytes.b1

' A UNION member can be a nested TYPE (structurally allowed per FB docs).
TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

UNION PointOrLong
    pt AS Point
    asLong AS LONGINT
END UNION

DIM pl AS PointOrLong
pl.pt.x = 7
pl.pt.y = 9
PRINT pl.pt.x
PRINT pl.pt.y

' A TYPE can also embed a UNION as a field (allowed the other way too).
TYPE Tagged
    tag AS INTEGER
    data AS IntBytes
END TYPE

DIM t AS Tagged
t.tag = 1
t.data.asInt = 258
PRINT t.data.bytes.b0
PRINT t.data.bytes.b1

' Size: a union's size is the size of its largest member (not the sum) -
' verified indirectly here by writing the wide member and reading each
' narrow byte back through the shared memory.
DIM u2 AS IntBytes
u2.asInt = 65535
PRINT u2.bytes.b0
PRINT u2.bytes.b1
PRINT u2.bytes.b2
