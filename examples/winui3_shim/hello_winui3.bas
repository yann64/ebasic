' Drives a real WinUI3 (Windows App SDK) window from eBasic - see this
' directory's README.md for the full pattern (why a separate process,
' not an in-process DLL) and how to build host/eb_winui3_host.vcxproj
' first. Run this from the repository root, matching every other example
' in examples/, since the launched path below is relative to that.
'
' ShellExecuteA (Stdcall, real Win32 - shell32.dll) rather than the lower-
' level CreateProcessA: no STARTUPINFOA/PROCESS_INFORMATION struct layout
' to replicate on the eBasic side, just plain scalar/ZSTRING parameters -
' the right tool for "launch this program with these arguments" here.
Extern "C" Lib "shell32"
    Declare Function ShellExecuteA Stdcall (ByVal hwnd AS ANY PTR, ByVal operation AS ZSTRING, ByVal file AS ZSTRING, ByVal params AS ZSTRING, ByVal directory AS ZSTRING, ByVal showCmd AS INTEGER) AS ANY PTR
End Extern

DIM title AS STRING
title = "Hello from eBasic"

DIM message AS STRING
message = "This WinUI3 window was launched by a real eBasic program, via ShellExecuteA (Stdcall/EXTERN)."

DIM quote AS STRING
quote = Chr(34)
DIM params AS STRING
params = quote & title & quote & " " & quote & message & quote

DIM result AS ANY PTR
result = ShellExecuteA(0, "open", "examples\winui3_shim\host\x64\Debug\eb_winui3_host.exe", params, "", 1)

' ShellExecuteA returns a value > 32 on success, an error code otherwise -
' see docs.microsoft.com's own ShellExecute reference for the exact codes.
PRINT "launched"
