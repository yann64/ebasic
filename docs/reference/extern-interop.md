# `EXTERN` / C-C++ Interop

Binds eBasic code to a real, separately-compiled C or C++ library - no body
is written; the real implementation lives externally, and the generated C++
declares a matching prototype and links against it (via `-l<name>` from a
`Lib` clause).

## `Extern "C" ... End Extern`

```
Extern "C" Lib "libname"
    Declare Sub Name(params...)
    Declare Function Name(params...) AS ReturnType
    ...
End Extern
```

Each `Declare` line inside the block is a bodyless signature bound to a real
C symbol of the same name (unless overridden with `Alias`, below). `Lib
"libname"` names the library to link (`-llibname`, no `lib`/`.so`/`.a`
affixes) - every `Declare` in the block shares it.

```basic
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_add(ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
End Extern

PRINT eb_fixture_add(2, 3)   ' 5
```

## Standalone `Declare` (no block)

```
Declare Sub Name [Cdecl] [Lib "libname"] [Alias "realName"] (params...)
Declare Function Name [Cdecl] [Lib "libname"] [Alias "realName"] (params...) AS ReturnType
```

A `Declare` doesn't have to sit inside an `Extern` block - written at the
top level on its own, it always binds with `"C"` linkage (a standalone
`Declare` can't produce `"C++"` linkage; that's only reachable via an
`Extern "C++"` block). **The clause order matters**: `Cdecl`/`Lib`/`Alias`
all come *before* the parameter list, matching real FreeBASIC's own
grammar - `Lib "name" (params)`, not `(params) Lib "name"`.

```basic
Declare Function eb_fixture_add Lib "ebfixturec" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
PRINT eb_fixture_add(2, 3)   ' 5
```

## `Alias`

Binds an eBasic-side name to a *different* real external symbol name -
needed whenever the real name isn't a valid/desired eBasic identifier, or to
give two differently-typed eBasic overloads of the same real C++ overloaded
function:

```basic
Extern "C++" Lib "ebfixturecpp"
    Namespace ebfixture
        Declare Function AddInts Alias "Add" (ByVal a AS INTEGER, ByVal b AS INTEGER) AS INTEGER
        Declare Function AddDoubles Alias "Add" (ByVal a AS DOUBLE, ByVal b AS DOUBLE) AS DOUBLE
    End Namespace
End Extern

PRINT ebfixture.AddInts(2, 3)        ' 5
PRINT ebfixture.AddDoubles(2.5, 3.5) ' 6
```

## `Extern "C++"` and `Namespace` (nested)

```
Extern "C++" Lib "libname"
    Namespace RealNamespaceName
        Declare Function Name(params...) AS ReturnType
    End Namespace
End Extern
```

A `Namespace` block nested inside `Extern "C++"` binds to a real C++
namespaced function - called from eBasic the same way as an ordinary
[`NAMESPACE`](namespaces-pointers-unions.md#namespace) member,
`RealNamespaceName.Name(...)`.

```basic
Extern "C++" Lib "ebfixturecpp"
    Namespace ebfixture
        Declare Function Square(ByVal x AS INTEGER) AS INTEGER
    End Namespace
End Extern

PRINT ebfixture.Square(5)   ' 25
```

## `ZSTRING` at the interop boundary

A `STRING` argument converts to/from a `ZSTRING`/`ZSTRING PTR` parameter
automatically - no manual marshaling needed at the call site:

```basic
Extern "C" Lib "ebfixturec"
    Declare Function eb_fixture_greeting() AS ZSTRING
    Declare Function eb_fixture_strlen_like(ByVal s AS ZSTRING) AS INTEGER
End Extern

DIM greeting AS STRING
greeting = eb_fixture_greeting()          ' ZSTRING (const char*) -> STRING
PRINT greeting

DIM mine AS STRING
mine = "hello there"
PRINT eb_fixture_strlen_like(mine)        ' STRING -> ZSTRING
```

A C function documented to return `NULL` on failure converts to an empty
`STRING`, not a crash - `ZSTRING`-returning calls are null-safe by
construction.

## Opaque "handle" types

```
TYPE Handle
END TYPE
```

A `TYPE` with **zero fields and zero methods** is an opaque external handle
- the classic C "pointer to an incomplete struct" idiom (as used by SQLite,
SDL, and many other real C libraries), where the caller only ever holds a
pointer and never needs (or is allowed) to know the real layout. It may
**only** be used behind a `PTR` - a by-value `DIM` of an opaque type is a
Sema error (`cannot be a by-value DIM of opaque external TYPE '<name>'
(unknown layout - declare it as '<name> PTR' instead)`).

```basic
TYPE Handle
END TYPE

Extern "C" Lib "ebfixturec"
    Declare Function HandleCreate Alias "eb_fixture_handle_create" (ByVal initial AS INTEGER) AS Handle PTR
    Declare Function HandleGet Alias "eb_fixture_handle_get" (ByVal h AS Handle PTR) AS INTEGER
    Declare Sub HandleDestroy Alias "eb_fixture_handle_destroy" (ByVal h AS Handle PTR)
End Extern

DIM h AS Handle PTR
h = HandleCreate(10)
PRINT HandleGet(h)     ' 10
CALL HandleDestroy(h)
```

## See also

- [Types and Literals](types-and-literals.md) - `ZSTRING`
- [Namespaces, Pointers, and Unions](namespaces-pointers-unions.md) - `PTR`, `NAMESPACE`
- [End-user guide: `ebpm`](../guide/ebpm.md) - linking against a package's own auto-generated interface uses this exact same `Extern "C++"`/`Declare` machinery
