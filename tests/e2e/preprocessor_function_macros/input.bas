' Function-like #define (with a variadic parameter), the # stringize and
' ## concatenate operators - Tier-1/2 additions matching real FreeBASIC's
' own documented examples (define.bas/stringize.bas/concat.bas).

#define MyMul(A, B) ((A) * (B))
Print MyMul(6, 7)

' Parameters are substituted textually, so an argument that's itself an
' expression is re-evaluated wherever the parameter name appears.
#define Square(n) ((n) * (n))
Print Square(2 + 3)

' # stringize: converts a macro argument into a string literal of its own
' source text.
#define SEE(x) Print #x, " = ", x
Dim variable AS INTEGER
variable = 1
SEE(variable)

' ## concatenate: pastes two adjacent tokens into one, which can then form
' a real identifier.
#define Concat(t, n) t##n
Dim helloworld AS STRING
helloworld = "pasted"
Print Concat(hello, world)

' A variadic parameter collects every trailing argument, still
' comma-joined, so a macro body can re-split it itself.
#define CountArgs(first, rest...) first
Print CountArgs(1, 2, 3, 4)

' Recursive macro expansion: a macro's body may itself use another macro.
#define BASE 10
#define DOUBLE_BASE (BASE * 2)
Print DOUBLE_BASE
