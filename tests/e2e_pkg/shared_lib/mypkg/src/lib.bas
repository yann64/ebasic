' Fixture for the e2e_pkg_shared_lib test (tests/e2e_pkg/run_shared_lib_case.sh):
' [shared-lib] target built via `ebpm build`, then dlopen'd from a C harness -
' see tests/fixtures/shared_lib/mylib.bas for the same shape used directly
' against ebc (this one is built through ebpm instead).
Extern "C"
    Function AddNumbers(a As Integer, b As Integer) As Integer
        AddNumbers = a + b
    End Function
End Extern

Function Greet(name As String) As String
    Greet = "Hello, " & name
End Function
