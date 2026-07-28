Extern "C++" Lib "ebfixturecpp"
    Namespace ebfixture
        Declare Function Square(ByVal x AS INTEGER) AS INTEGER
        Declare Function AddInts Alias "Add" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
        Declare Function AddDoubles Alias "Add" (ByVal a AS DOUBLE, ByVal b AS DOUBLE) AS DOUBLE
    End Namespace
End Extern

PRINT ebfixture.Square(5)
PRINT ebfixture.AddInts(2, 3)
PRINT ebfixture.AddDoubles(2.5, 3.5)
