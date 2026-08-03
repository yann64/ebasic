' eBasic's standard string library (see docs/reference/string-library.md) -
' every function's normal case, plus its documented edge cases (empty
' string, out-of-range counts/positions, not-found searches, unparseable
' Val input, n=0 for Repeat/Space).

PRINT Len("hello")
PRINT Len("")

PRINT Left("hello", 3)
PRINT Right("hello", 3)
PRINT Left("hi", 100)        ' count past the end - clamped, not an error
PRINT Right("hi", 100)
PRINT Left("hi", -1)         ' negative count - clamped to ""
PRINT Right("hi", 0)

PRINT Mid("hello world", 7, Len("hello world"))   ' "to the end" via Len()
PRINT Mid("hello", 2, 3)
PRINT Mid("hello", 100, 5)   ' start past the end - ""
PRINT Mid("hello", 3, 2147483647) ' default-length sentinel directly

PRINT InStr("hello world", "world")
PRINT InStr("hello", "xyz")           ' not found - 0
PRINT InStr("aXbXc", "X", 3)          ' explicit start position
PRINT InStrRev("abcabc", "abc")
PRINT InStrRev("hello", "xyz")        ' not found - 0

PRINT UCase("Hello, World!")
PRINT LCase("Hello, World!")

PRINT LTrim("  hi")
PRINT RTrim("hi  ")
PRINT Trim("  hi  ")
PRINT Trim("**hi**", "*")             ' custom trim character set

PRINT Str(5)
PRINT Str(3.14)
PRINT Str(-1)
PRINT Str(0)

PRINT Val("42abc")
PRINT Val("abc")                      ' unparseable - 0
PRINT Val("  3.5")                    ' leading whitespace skipped
PRINT Val("")

PRINT Chr(65)
PRINT Asc("Ace")
PRINT Asc("")                         ' empty string - 0

PRINT Space(3)
PRINT Space(0)

PRINT Repeat(3, "ab")
PRINT Repeat(0, "ab")                 ' n=0 - ""
