' Stdcall on an eBasic-defined callback: a plain top-level FUNCTION marked
' Stdcall, address-taken via @ProcName, passed to a real C function whose
' callback parameter is genuinely __stdcall-typed (eb_fixture_invoke_
' stdcall_comparator, fixture.c) - mirrors tests/e2e/extern_stdcall's own
' rationale exactly: proves caller and callee genuinely agree on calling
' convention, not just that both sides compile. A no-op everywhere off
' Windows/x86 (EBASIC_STDCALL expands to nothing there, same as every
' other Stdcall use in this compiler), so this also just confirms correct
' compilation on every other platform.
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_invoke_stdcall_comparator Cdecl (ByVal cmp AS FUNCTION Stdcall (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER, ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

FUNCTION Compare Stdcall (BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    IF a < b THEN
        RETURN -1
    ELSEIF a > b THEN
        RETURN 1
    ELSE
        RETURN 0
    END IF
END FUNCTION

PRINT eb_fixture_invoke_stdcall_comparator(@Compare, 3, 7)
PRINT eb_fixture_invoke_stdcall_comparator(@Compare, 7, 3)
PRINT eb_fixture_invoke_stdcall_comparator(@Compare, 5, 5)

' Stored in a DIM'd Stdcall function-pointer variable and called directly
' from eBasic code too (not just handed to external code).
DIM cb AS FUNCTION Stdcall (BYVAL AS INTEGER, BYVAL AS INTEGER) AS INTEGER
cb = @Compare
PRINT cb(2, 9)
