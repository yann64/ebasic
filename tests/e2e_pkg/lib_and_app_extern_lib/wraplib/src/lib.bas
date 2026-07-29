' Wraps a real, separately-compiled C library (ebfixturec, shared with the
' plain e2e/extern_c test) behind an ordinary top-level FUNCTION - exactly
' the "raw Extern layer wrapped by an idiomatic top-level function" shape a
' real native-library binding package (e.g. a GTK4 wrapper) uses. Proves
' `ebc --lib`'s .libs sidecar + ebpm's transitive-lib forwarding actually
' gets `wraplib`'s own `Lib "ebfixturec"` need to a downstream consumer's
' final link step, since wraplib.iface.bas alone never mentions it.

Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_add(ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

FUNCTION WrappedAdd(a AS INTEGER, b AS INTEGER) AS INTEGER
    WrappedAdd = eb_fixture_add(a, b)
END FUNCTION
