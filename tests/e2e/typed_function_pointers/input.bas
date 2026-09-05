' Typed function-pointer EXTERN/DECLARE parameters: proves the full round
' trip (parse -> sema signature match -> real C function-pointer codegen,
' never void*) via ebfixturec's eb_fixture_invoke_comparator, distinct from
' the untyped ANY-PTR-based eb_fixture_invoke_callback already covered by
' the function_pointers case. Also covers storing a typed function pointer
' in a DIM'd variable and a TYPE field (not just an inline @ProcName
' argument), bridging a FunctionPointer-typed value through a bare
' ANY PTR variable and back, and calling through a stored function
' pointer directly from eBasic code (both as an expression and as a
' statement, and through a function-pointer parameter inside a
' higher-order FUNCTION - no external C code involved in any of these).
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_invoke_comparator Cdecl (ByVal cmp AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER, ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

FUNCTION Compare(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    IF a < b THEN
        RETURN -1
    ELSEIF a > b THEN
        RETURN 1
    ELSE
        RETURN 0
    END IF
END FUNCTION

TYPE Callbacks
    cmp AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
END TYPE

' Inline @ProcName argument.
PRINT eb_fixture_invoke_comparator(@Compare, 3, 7)

' Stored in a DIM'd function-pointer variable.
DIM cb AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
cb = @Compare
PRINT eb_fixture_invoke_comparator(cb, 7, 3)

' Stored in a TYPE field.
DIM c AS Callbacks
c.cmp = @Compare
PRINT eb_fixture_invoke_comparator(c.cmp, 5, 5)

' Bridged through a bare ANY PTR variable and back (FunctionPointer <->
' ANY PTR, both directions).
DIM raw AS ANY PTR
raw = cb
PRINT eb_fixture_invoke_comparator(raw, 2, 9)

' Calling through a stored function pointer directly from eBasic code -
' no external C code involved.
PRINT cb(8, 1)      ' expression position
CALL cb(4, 4)        ' statement position (return value discarded)

FUNCTION ApplyTwice(BYVAL f AS FUNCTION (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER, BYVAL x AS INTEGER) AS INTEGER
    ' Calling through a function-pointer *parameter* - proves parameters
    ' flow through the same lookup as locals, no special-casing needed.
    RETURN f(f(x, 1), x)
END FUNCTION

PRINT ApplyTwice(@Compare, 5)
