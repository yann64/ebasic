' #macro/#endmacro: the multi-line version of a function-like #define,
' matching real FreeBASIC's own documented examples (macro.bas/macro2.bas/
' macro3.bas). Only supported as the entire content of a source line (see
' docs/reference/preprocessor.md) - unlike a function-like #define, a
' #macro's body may itself contain real directives, evaluated at
' invocation time using that call's own argument bindings.

#macro Print2(a, b)
    Print a
    Print b
#endmacro

Print2("Hello", "World")

' A variadic parameter, tested with the stringize operator to detect a
' missing trailing argument - exactly FreeBASIC's own test1 example.
#macro test1(arg1, arg2...)
    Print arg1
    #if #arg2 = ""
        Print "2nd argument not passed"
    #else
        Print arg2
    #endif
#endmacro

test1("1", "2")
Print "-----"
test1("3")
Print "-----"
test1(5, 6)
