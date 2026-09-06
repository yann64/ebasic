' M10: generics - a single type parameter on a free FUNCTION/SUB, inferred
' from a call-site argument's own type, instantiated (cloned + substituted)
' once per distinct concrete type and deduplicated across repeat calls.

FUNCTION Max(OF T) (a AS T, b AS T) AS T
    IF a > b THEN
        Max = a
    ELSE
        Max = b
    END IF
END FUNCTION

SUB PrintTwice(OF T) (v AS T)
    PRINT v
    PRINT v
END SUB

TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

' A UserDefined-typed instantiation exercises the BYREF-recomputation path
' (Point defaults to BYREF, unlike INTEGER/DOUBLE above) and confirms the
' substitution walk reaches a field access inside the generic body (only
' valid once T is concretely Point).
SUB BumpX(OF T) (p AS T)
    p.x = p.x + 100
END SUB

PRINT Max(1, 2)
PRINT Max(5, 3)
PRINT Max(1, 2)     ' same instantiation again - proves dedup, not a redefinition error
PRINT Max(1.5, 2.5)

CALL PrintTwice(42)

DIM pt AS Point
pt.x = 10
pt.y = 20
CALL BumpX(pt)      ' BYREF: mutates the caller's own pt
PRINT pt.x
