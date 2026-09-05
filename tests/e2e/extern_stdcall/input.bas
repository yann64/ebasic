' M8f: Stdcall calling convention on EXTERN/DECLARE - eb_fixture_stdcall_add
' is genuinely compiled __stdcall (see tests/fixtures/c/fixture.c), so this
' also proves caller/callee actually agree on calling convention (a real
' ABI detail), not just that both sides compile.

Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_stdcall_add Stdcall (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

PRINT eb_fixture_stdcall_add(2, 3)

' Standalone Declare form (no Extern block) - real FreeBASIC's own clause
' order: Name Stdcall Lib "name" (params) As type. Aliased to the same real
' symbol as above (a second Declare naming the same C function under a
' different eBasic-side name, exactly like extern_cpp's AddInts/AddDoubles
' pattern) rather than needing a second fixture function.
Declare Function eb_fixture_stdcall_add2 Stdcall Lib "ebfixturec" Alias "eb_fixture_stdcall_add" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
PRINT eb_fixture_stdcall_add2(10, 20)
