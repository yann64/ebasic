Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_add(ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
    Declare Function eb_fixture_greeting() AS ZSTRING
    Declare Function eb_fixture_strlen_like(ByVal s AS ZSTRING) AS INTEGER
    Declare Function eb_fixture_maybe_null(ByVal flag AS INTEGER) AS ZSTRING
    Declare Function eb_fixture_malloc_string() AS ANY PTR
    Declare Sub eb_fixture_free(ByVal p AS ANY PTR)
    Declare Function eb_fixture_sum_lengths(ByVal arr AS ANY PTR, ByVal count AS INTEGER) AS INTEGER
End Extern

PRINT eb_fixture_add(2, 3)

DIM greeting AS STRING
greeting = eb_fixture_greeting()
PRINT greeting

DIM mine AS STRING
mine = "hello there"
PRINT eb_fixture_strlen_like(mine)

DIM notNull AS STRING
notNull = eb_fixture_maybe_null(1)
PRINT notNull

DIM wasNull AS STRING
wasNull = eb_fixture_maybe_null(0)
PRINT "[" & wasNull & "]"

' ANY PTR -> ZSTRING bridge: reads a malloc'd buffer (handed back as a
' plain void*) as a real string, then frees the original ANY PTR value -
' the whole point of the bridge is that both steps are expressible without
' ever needing a dedicated CAST operator.
DIM rawPtr AS ANY PTR
rawPtr = eb_fixture_malloc_string()
DIM viaZstring AS ZSTRING
viaZstring = rawPtr
DIM copied AS STRING
copied = viaZstring
CALL eb_fixture_free(rawPtr)
PRINT copied

' Regression test for the string-literal-to-ZSTRING dangling-pointer bug:
' each element is assigned in its own statement (not consumed within the
' same expression as the literal), then read back afterward, across a
' function-call boundary - exactly the shape that used to corrupt.
DIM names(2) AS ZSTRING
names(0) = "echo"
names(1) = "hello"
names(2) = "world"
PRINT eb_fixture_sum_lengths(@names(0), 3)
