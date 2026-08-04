' See docs/reference/extern-interop.md
' Binds directly to the real C standard library - no extra -L/-l needed,
' since libc is always linked.

Extern "C"
    Declare Function c_abs Alias "abs" (ByVal n AS INTEGER) AS INTEGER
    Declare Function atoi(ByVal s AS ZSTRING) AS INTEGER
End Extern

PRINT c_abs(-5)

DIM numText AS STRING
numText = "42"
PRINT atoi(numText)
