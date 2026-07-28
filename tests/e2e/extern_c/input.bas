Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_add(ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
    Declare Function eb_fixture_greeting() AS ZSTRING
    Declare Function eb_fixture_strlen_like(ByVal s AS ZSTRING) AS INTEGER
    Declare Function eb_fixture_maybe_null(ByVal flag AS INTEGER) AS ZSTRING
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
