TYPE Node
    value AS INTEGER
    nxt AS Node PTR
END TYPE

DIM n1 AS Node
DIM n2 AS Node
DIM n3 AS Node

n1.value = 10
n2.value = 20
n3.value = 30

n1.nxt = @n2
n2.nxt = @n3
n3.nxt = 0

DIM p AS Node PTR
p = @n1

DO WHILE p <> 0
    PRINT p->value
    p = p->nxt
LOOP

' (*p).field is equivalent to p->field
p = @n1
PRINT (*p).value

' ANY PTR: untyped pointer, comparable to 0
DIM anyP AS ANY PTR
anyP = @n1
PRINT anyP <> 0
anyP = 0
PRINT anyP <> 0

' Pointer arithmetic through an array
DIM arr(4) AS INTEGER
arr(0) = 100
arr(1) = 200
arr(2) = 300

DIM ip AS INTEGER PTR
ip = @arr(0)
PRINT *ip
PRINT *(ip + 1)
ip = ip + 1
PRINT *ip

' Deref-assignment through a pointer
*ip = 999
PRINT arr(1)

' Pointer difference (in elements)
DIM ip0 AS INTEGER PTR
DIM ip2 AS INTEGER PTR
ip0 = @arr(0)
ip2 = @arr(2)
PRINT ip2 - ip0

' ANY PTR -> typed PTR: Sema documents/permits this bridge (see
' docs/reference/namespaces-pointers-unions.md), but it needs Codegen to
' emit an explicit static_cast (C++ has no implicit void* -> T* conversion,
' unlike the reverse T* -> void* direction already exercised above).
anyP = @n1

' Variable assignment
DIM typedFromAny AS Node PTR
typedFromAny = anyP
PRINT typedFromAny->value

' Member/field assignment
n3.nxt = anyP
PRINT n3.nxt->value

' Call-argument bridging
FUNCTION TakesTyped(BYVAL n AS Node PTR) AS INTEGER
    TakesTyped = n->value
END FUNCTION
PRINT TakesTyped(anyP)

' Return-assign bridging (FB's implicit `FuncName = value` return form)
FUNCTION MakeTyped() AS Node PTR
    MakeTyped = anyP
END FUNCTION
DIM viaReturn AS Node PTR
viaReturn = MakeTyped()
PRINT viaReturn->value
