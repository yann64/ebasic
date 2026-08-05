' Fixture for the --shared-lib/-dll e2e test (tests/cli/shared_lib.sh):
' one real, stable, unmangled C export (only Sub/Function definitions
' written inside Extern "C" ... End Extern become one), plus one plain
' internal helper using STRING - proving STRING can still be used freely
' outside the exported/C-ABI boundary, just not across it.
Extern "C"
    Function AddNumbers(a As Integer, b As Integer) As Integer
        AddNumbers = a + b
    End Function
End Extern

Function Greet(name As String) As String
    Greet = "Hello, " & name
End Function
