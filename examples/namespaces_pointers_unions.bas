' See docs/reference/namespaces-pointers-unions.md

NAMESPACE MathUtils
    CONST PI AS SINGLE = 3.5

    FUNCTION Square(n AS INTEGER) AS INTEGER
        Square = n * n
    END FUNCTION
END NAMESPACE

PRINT MathUtils.PI
PRINT MathUtils.Square(5)

TYPE Node
    value AS INTEGER
    nxt AS Node PTR
END TYPE

DIM n1 AS Node
DIM n2 AS Node
n1.value = 10
n2.value = 20
n1.nxt = @n2
n2.nxt = 0

DIM p AS Node PTR
p = @n1
DO WHILE p <> 0
    PRINT p->value
    p = p->nxt
LOOP

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
u.asInt = 1
PRINT u.bytes.b0
