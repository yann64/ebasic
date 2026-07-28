TYPE Handle
END TYPE

Extern "C" Lib "ebfixturec"
    Declare Function HandleCreate Alias "eb_fixture_handle_create" (ByVal initial AS INTEGER) AS Handle PTR
    Declare Function HandleGet Alias "eb_fixture_handle_get" (ByVal h AS Handle PTR) AS INTEGER
    Declare Sub HandleAdd Alias "eb_fixture_handle_add" (ByVal h AS Handle PTR, ByVal delta AS INTEGER)
    Declare Sub HandleDestroy Alias "eb_fixture_handle_destroy" (ByVal h AS Handle PTR)
End Extern

DIM h AS Handle PTR
h = HandleCreate(10)
PRINT HandleGet(h)
CALL HandleAdd(h, 5)
PRINT HandleGet(h)
CALL HandleDestroy(h)
