' __LINE__/__FILE__/__DATE__/__TIME__: predefined, dynamically-substituted
' macros (unlike __FB_WIN32__ etc, these vary per use site/per compile, so
' they can't just be seeded once into the macro table - see expandText in
' preprocessor.cpp). __DATE__/__TIME__'s actual values are compile-time-
' dependent, so this only checks their documented format's length
' ("mm-dd-yyyy" = 10 chars, "hh:mm:ss" = 8), never the literal value -
' otherwise the test would fail whenever it's re-run on a later date.

Print __LINE__
Print __LINE__
Print InStr(__FILE__, "input.bas") > 0
Print Len(__DATE__)
Print Len(__TIME__)

#define WhereAmI() __LINE__
Print WhereAmI()
